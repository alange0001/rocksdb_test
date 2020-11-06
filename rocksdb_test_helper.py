#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
@author: Adriano Lange <alange0001@gmail.com>

Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
This source code is licensed under both the GPLv2 (found in the
LICENSE file in the root directory) and Apache 2.0 License
(found in the LICENSE.Apache file in the root directory).
"""

import os
import sys
import signal
import traceback
import subprocess
import argparse
import json
import collections

#=============================================================================
import logging
log = logging.getLogger('rocksdb_test_helper')
log.setLevel(logging.INFO)

#=============================================================================
class ArgsWrapper: # single global instance "args"
	def get_args(self):
		preparser = argparse.ArgumentParser("rocksdb_test helper", add_help=False)
		preparser.add_argument('-l', '--log_level', type=str,
			default='INFO', choices=[ 'debug', 'DEBUG', 'info', 'INFO' ],
			help='log level')
		preparser.add_argument('--load_args', type=str,
			default=None,
			help='load arguments from file')
		preparser.add_argument('--save_args', type=str,
			default=None,
			help='save arguments to a file')
		preargs, remainargv = preparser.parse_known_args()

		log_h = logging.StreamHandler()
		#log_h.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s: %(message)s'))
		log_h.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
		log.addHandler(log_h)
		log.setLevel(getattr(logging, preargs.log_level.upper()))

		log.debug(f'Preargs: {str(preargs)}')
		
		if preargs.load_args is not None:
			log.info(f'Loading arguments from file "{preargs.load_args}".')
			with open(preargs.load_args, 'r') as fd:
				load_args = json.load(fd)
			log.info(f'    loaded arguments: {load_args}')
		else:
			load_args = {}
			
		parser = argparse.ArgumentParser(
			description="rocksdb_test helper")
		parser.add_argument('-l', '--log_level', type=str,
			default='INFO', choices=[ 'debug', 'DEBUG', 'info', 'INFO' ],
			help='log level')
		parser.add_argument('--load_args', type=str,
			default=None,
			help='load arguments from file')
		parser.add_argument('--save_args', type=str,
			default=None,
			help='save arguments to a file')
		
		parser.add_argument('--data_path', type=str,
			default=coalesce(load_args.get('data_path'), '/media/auto/work'),
			help='the mount point of "--io_device" used to store database experiments')
		parser.add_argument('--backup_path', type=str,
			default=coalesce(load_args.get('backup_path'), '/media/auto/work2'),
			help='directory used to store database backups')
		
		parser.add_argument('--rocksdb_test_path', type=str,
			default=load_args.get('rocksdb_test_path'),
			help='directory of rocksdb_test')
		parser.add_argument('--rocksdb_path', type=str,
			default=load_args.get('rocksdb_path'),
			help='directory of rocksdb')
		parser.add_argument('--ycsb_path', type=str,
			default=load_args.get('ycsb_path'),
			help='directory of YCSB')
		
		parser.add_argument('--confirm_cmd', type=bool,
			default=coalesce(load_args.get('confirm_cmd'), False),
			help='confirm before execution')
		
		parser.add_argument('--test', type=str,
			default='',
			help='test routines')
		
		subparsers = parser.add_subparsers(dest='experiment', title='experiments', description='Experiments available')
		parser_ycsb_at3 = subparsers.add_parser('ycsb_at3', help='ycsb benchmark + access_time3')
		Exp_ycsb_at3.set_args(parser_ycsb_at3, load_args)
		
		args = parser.parse_args(remainargv)
		log.debug(f'Arguments: {str(args)}')

		if preargs.save_args is not None:
			args_d = collections.OrderedDict()
			for k in dir(args):
				if k[0] != '_' and k not in ['test', 'log_level', 'save_args', 'load_args']:
					args_d[k] = getattr(args, k)
			log.info(f'saving arguments to file: {preargs.save_args}')
			with open(preargs.save_args, 'w') as f:
				json.dump(args_d, f)
				f.write('\n')
		
		return args

	def __getattr__(self, name):
		global args
		args = self.get_args()
		return getattr(args, name)

args = ArgsWrapper()

#=============================================================================
def main():
	if args.experiment == 'ycsb_at3':
		Exp_ycsb_at3().run()
	
	return 0

#=============================================================================
class GenericExperiment:
	exp_params = collections.OrderedDict([
		#Name,                   Type       Def   Help
		('docker_image',          {'type':str,  'default':None, 'help':'docker image' }),
		('docker_params',         {'type':str,  'default':None, 'help':'additional docker arguments' }),
		('duration',              {'type':int,  'default':None, 'help':'duration of the experiment (minutes)' }),
		('warm_period',           {'type':int,  'default':None, 'help':'warm period (minutes)' }),
		('io_device',             {'type':str,  'default':None, 'help':'disk device name' }),
		('rocksdb_config_file',   {'type':str,  'default':None, 'help':None }),
		('num_dbs',               {'type':int,  'default':   0, 'help':None }),
		('db_num_keys',           {'type':str,  'default':None, 'help':None }),
		('db_path',               {'type':str,  'default':None, 'help':None }),
		('db_benchmark',          {'type':str,  'default':None, 'help':None }),
		('db_threads',            {'type':str,  'default':None, 'help':None }),
		('db_cache_size',         {'type':str,  'default':None, 'help':None }),
		('num_ydbs',              {'type':int,  'default':   0, 'help':None }),
		('ydb_num_keys',          {'type':str,  'default':None, 'help':None }),
		('ydb_path',              {'type':str,  'default':None, 'help':None }),
		('ydb_threads',           {'type':str,  'default':None, 'help':None }),
		('ydb_workload',          {'type':str,  'default':None, 'help':None }),
		('ydb_sleep',             {'type':str,  'default':None, 'help':None }),
		('num_at',                {'type':int,  'default':   0, 'help':None }),
		('at_dir',                {'type':str,  'default':None, 'help':None }),
		('at_file',               {'type':str,  'default':None, 'help':None }),
		('at_block_size',         {'type':str,  'default':None, 'help':None }),
		('at_params',             {'type':str,  'default':None, 'help':None }),
		('at_script',             {'type':str,  'default':None, 'help':None }),
		('params',                {'type':str,  'default':None, 'help':None }),
		('output',                {'type':str,  'default':None, 'help':None }),
		])

	@classmethod
	def set_args(cls, parser, load_args):
		for k, v in cls.exp_params.items():
			parser.add_argument(f'--{k}', type=v['type'], default=coalesce(load_args.get(k), v['default']),
				help=v['help'])
			
	def get_args_d(self):
		return self.process_args_d( args_to_dir(args) )
	
	def process_args_d(self, args_d):
		if args_d['num_dbs'] > 0:
			args_d['db_num_keys']   = coalesce(args_d.get('db_num_keys'), 500000000 )
			args_d['db_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_{x}' for x in range(0, args_d['num_dbs']) ])
			args_d['db_benchmark']  = coalesce(args_d.get('db_benchmark'), 'readwhilewriting' )
			args_d['db_threads']    = coalesce(args_d.get('db_threads'), 1 )
			args_d['db_cache_size'] = coalesce(args_d.get('db_cache_size'), 536870912 )

		if args_d['num_ydbs'] > 0:
			args_d['ydb_num_keys'] = coalesce(args_d.get('ydb_num_keys'), 50000000 )
			args_d['ydb_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_ycsb_{x}' for x in range(0, args_d['num_ydbs']) ])
			args_d['ydb_threads']  = coalesce(args_d.get('ydb_threads'), 1 )
			args_d['ydb_workload'] = coalesce(args_d.get('ydb_workload'), 'workloadb' )
			args_d['ydb_sleep']    = coalesce(args_d.get('ydb_sleep'), 0 )
			
		if args_d['num_at'] > 0:
			args_d['at_dir']        = coalesce(args_d.get('at_dir'), f'{args_d["data_path"]}/tmp' )
			args_d['at_file'] = '#'.join([ str(x) for x in range(0, args_d['num_at']) ])
			args_d['at_block_size'] = coalesce(args_d.get('at_block_size'), 512 )
			
		docker_default_path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
		docker_add_path = "/workspace/rocksdb_test/build:/workspace/YCSB/bin:/workspace/rocksdb"
		
		docker_params = [coalesce(args_d.get('docker_params'), '')]
		for k, v in [('rocksdb_test_path', '/workspace/rocksdb_test'),
		             ('rocksdb_path', '/workspace/rocksdb'),
		             ('ycsb_path', '/workspace/YCSB')]:
			if args_d.get(k) is not None:
				docker_params.append(f'-v {args_d[k]}:{v}')
		docker_params.append(f"-e PATH={docker_default_path}:{docker_add_path}")
		args_d['docker_params'] = ' '.join(docker_params)

		return args_d
		
	def run(self, args_d=None):
		log.info('')
		log.info('==========================================')
		args_d = coalesce( args_d, self.get_args_d() )
		
		cmd = 'build/rocksdb_test \\\n'
		cmd += f'	--log_level="info"  \\\n'
		cmd += f'	--stats_interval=5  \\\n'

		def_p_func = lambda k, v: f'	--{k}="{args_d[k]}" \\\n'
		self.exp_params['ydb_workload']['p_func'] = lambda k, v: f'	--{k}="/workspace/YCSB/workloads/{args_d[k]}" \\\n'
		self.exp_params['params']['p_func'] = lambda k, v: f'	{args_d[k]}'
		self.exp_params['output']['p_func'] = lambda k, v: f' > "{args_d[k]}"'
		
		for k, v in self.exp_params.items():
			log.info(f'{k:<20} = {coalesce(args_d.get(k), "")}')
			if args_d.get(k) is not None:
				p_func = coalesce(v.get('p_func'), def_p_func)
				cmd += p_func(k, v)
				
		log.debug(cmd)
		
	def get_at3_script(self, wait, instances, interval):
		ret = []
		for i in range(0, instances):
			jc = wait + i * interval
			ret_l = f"0:wait;0:write_ratio=0;{jc}m:wait=false"
			for j in [0.1, 0.2, 0.3, 0.5, 0.7, 1]:
				jc += interval * instances
				ret_l += f";{jc}m:write_ratio={j}"
			ret.append(ret_l)
		return '#'.join(ret)

#=============================================================================
class Exp_ycsb_at3 (GenericExperiment):
	@classmethod
	def set_args(cls, parser, load_args):
		parser.add_argument('--ydb_workload_list', type=str, default=coalesce(load_args.get('ydb_workload_list'), 'workloadb workloada'),
			help='list of YCSB workloads (space separated)')
		parser.add_argument('--at_block_size_list', type=str, default=coalesce(load_args.get('at_block_size_list'), '512 4'),
			help='list of access_time3\'s block sizes (space separated)')
		parser.add_argument('--at_interval', type=int, default=coalesce(load_args.get('at_interval'), 2),
			help='interval between changes in the access_time3\' access pattern')
		
		cls.exp_params = cls.exp_params.copy()
		def replace_def(k, d):
			cls.exp_params[k] = cls.exp_params[k].copy()
			cls.exp_params[k]['default'] = d
		replace_def('duration'    , 90)
		replace_def('warm_period' , 30)
		replace_def('num_ydbs'    , 1)
		replace_def('ydb_threads' , 5)
		replace_def('num_at'      , 4)
		replace_def('at_params'   , '--flush_blocks=0 --random_ratio=0.5 --wait --direct_io')

		super(Exp_ycsb_at3, cls).set_args(parser, load_args)
	
	def run(self):
		args_d = self.get_args_d()
		
		args_d['at_script'] = self.get_at3_script(int(args_d['warm_period'])+10, int(args_d['num_at']), 2)
		
		for at_bs in args_d['at_block_size_list'].split(' '):
			args_d['at_block_size'] = at_bs
			for ydb_workload in args_d['ydb_workload_list'].split(' '):
				args_d['ydb_workload'] = ydb_workload
				args_d['output'] = f'ycsb_{ydb_workload},at3_bs{at_bs}_directio.out'
				super(Exp_ycsb_at3, self).run(args_d)

#=============================================================================
def command(cmd, raise_exception=True):
	log.debug(f'Executing command: {cmd}')
	err, out = subprocess.getstatusoutput(cmd)
	if err != 0:
		msg = f'erro {err} from command "{cmd}"'
		if raise_exception:
			raise Exception(msg)
		else:
			log.error(msg)
	return out

def test_dir(d):
	if not os.path.isdir(d):
		raise Exception(f'directory {d} does not exist')
	return d

def test_path(f):
	if not os.path.exists(f):
		raise Exception(f'path {f} does not exist')
	return f

def coalesce(*args):
	for v in args:
		if v is not None:
			return v
	return None

def args_to_dir(args):
	args_d = collections.OrderedDict()
	for k in dir(args):
		if k[0] != '_' and k not in ['test', 'log_level', 'save_args', 'load_args']:
			args_d[k] = getattr(args, k)
	return args_d


#=============================================================================
class Test:
	def __init__(self, name):
		f = getattr(self, name)
		if f is None:
			raise Exception(f'test named "{name}" does not exist')
		f()

	def args(self):
		log.info(args)

#=============================================================================
def signal_handler(signame, signumber, stack):
	log.warning("signal {} received".format(signame))
	exit(1)

#=============================================================================
if __name__ == '__main__':
	for i in ('SIGINT', 'SIGTERM'):
		signal.signal(getattr(signal, i),  lambda signumber, stack, signame=i: signal_handler(signame,  signumber, stack) )
		
	try:
		if args.test == '':
			exit( main() )
		else:
			Test(args.test)
			exit(0)

	except Exception as e:
		if log.level == logging.DEBUG:
			exc_type, exc_value, exc_traceback = sys.exc_info()
			sys.stderr.write('main exception:\n' + ''.join(traceback.format_exception(exc_type, exc_value, exc_traceback)) + '\n')
		else:
			sys.stderr.write(str(e) + '\n')
		exit(1)
