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
import psutil
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
			help='mount point used to store the databases')
		parser.add_argument('--output_path', type=str,
			default=coalesce(load_args.get('output_path'), ''),
			help='output directory of the experiments')
		
		parser.add_argument('--rocksdb_test_path', type=str,
			default=load_args.get('rocksdb_test_path'),
			help='directory of rocksdb_test')
		parser.add_argument('--rocksdb_path', type=str,
			default=load_args.get('rocksdb_path'),
			help='directory of rocksdb')
		parser.add_argument('--ycsb_path', type=str,
			default=load_args.get('ycsb_path'),
			help='directory of YCSB')
		
		log.debug(f"load_args.get('confirm_cmd') = {load_args.get('confirm_cmd')}")
		parser.add_argument('--confirm_cmd', type=str, action=self.__class__.BoolAction, nargs='?', const='true',
			default=coalesce(load_args.get('confirm_cmd'), False),
			help='confirm before each command execution')
		
		parser.add_argument('--test', type=str,
			default='',
			help='test routines')
		
		subparsers = parser.add_subparsers(dest='experiment', title='experiments', description='Experiments available')
		Exp_create_ycsb.register_subcommand(subparsers, load_args)
		Exp_ycsb.register_subcommand(subparsers, load_args)
		Exp_ycsb_at3.register_subcommand(subparsers, load_args)
		
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
	
	class BoolAction(argparse.Action):
		def __call__(self, parser, namespace, values, option_string):
			log.debug(f'BoolAction.__call__: {option_string}="{values}"')
			if values.lower() in ['', 'true', 't', 'yes', 'y', '1']:
				setattr(namespace, self.dest, True)
			elif values.lower() in ['false', 'f', 'no', 'n', '0']:
				setattr(namespace, self.dest, False)
			else:
				raise ValueError(f'invalid value for boolean argument {option_string}="{values}"')

args = ArgsWrapper()
experiment_list = collections.OrderedDict()

#=============================================================================
def main():
	exp_class = experiment_list.get(args.experiment)
	if exp_class is not None:
		exp_class().run()
	
	return 0

#=============================================================================
class GenericExperiment:
	exp_params = collections.OrderedDict([
		#Name,                   Type       Def   Help
		('docker_image',          {'type':str,  'default':None, 'help':'docker image' }),
		('docker_params',         {'type':str,  'default':None, 'help':'additional docker arguments' }),
		('duration',              {'type':int,  'default':None, 'help':'duration of the experiment (minutes)' }),
		('warm_period',           {'type':int,  'default':None, 'help':'warm period (minutes)' }),
		('rocksdb_config_file',   {'type':str,  'default':None, 'help':None }),
		('num_dbs',               {'type':int,  'default':0,    'help':None }),
		('db_num_keys',           {'type':str,  'default':None, 'help':None }),
		('db_path',               {'type':str,  'default':None, 'help':None }),
		('db_benchmark',          {'type':str,  'default':None, 'help':None }),
		('db_threads',            {'type':str,  'default':None, 'help':None }),
		('db_cache_size',         {'type':str,  'default':None, 'help':None }),
		('num_ydbs',              {'type':int,  'default':0,    'help':None }),
		('ydb_num_keys',          {'type':str,  'default':None, 'help':None }),
		('ydb_path',              {'type':str,  'default':None, 'help':None }),
		('ydb_threads',           {'type':str,  'default':None, 'help':None }),
		('ydb_workload',          {'type':str,  'default':None, 'help':None }),
		('ydb_sleep',             {'type':str,  'default':None, 'help':None }),
		('num_at',                {'type':int,  'default':0,    'help':None }),
		('at_dir',                {'type':str,  'default':None, 'help':None }),
		('at_file',               {'type':str,  'default':None, 'help':None }),
		('at_block_size',         {'type':str,  'default':None, 'help':None }),
		('at_params',             {'type':str,  'default':None, 'help':None }),
		('at_script',             {'type':str,  'default':None, 'help':None }),
		('perfmon',               {'type':str,  'default':None, 'help':'connect to performancemonitor' }),
		('perfmon_port',          {'type':int,  'default':None, 'help':'performancemonitor port' }),
		('params',                {'type':str,  'default':None, 'help':None }),
		('output',                {'type':str,  'default':None, 'help':None }),
		])

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		pass
		#global experiment_list
		#experiment_list['experiment_name'] = cls
		#parser = subparsers.add_parser('experiment_name', help='help')
		#cls.set_args(parser, load_args)

	@classmethod
	def set_args(cls, parser, load_args):
		parser.add_argument('--backup_dbbench', type=str, default=load_args.get('backup_dbbench'),
			help='restore db_bench backup from this .tar file (no subdir)')
		parser.add_argument('--backup_ycsb', type=str, default=load_args.get('backup_ycsb'),
			help='restore ycsb backup from this .tar file (no subdir)')

		parser.add_argument('--before_run_cmd', type=str, default=load_args.get('before_run_cmd'),
			help='command executed before running rocksdb_test')
		parser.add_argument('--after_run_cmd', type=str, default=load_args.get('after_run_cmd'),
			help='command executed after rocksdb_test')
		
		for k, v in cls.exp_params.items():
			parser.add_argument(f'--{k}', type=v['type'], default=coalesce(load_args.get(k), v['default']),
				help=v['help'])
	
	@classmethod
	def filter_args(cls, arg_names):
		filtered = collections.OrderedDict()
		for k in arg_names:
			filtered[k] = cls.exp_params[k].copy()
		cls.exp_params = filtered
			
	def get_args_d(self):
		return self.process_args_d( args_to_dir(args) )
	
	def process_args_d(self, args_d):
		if coalesce(args_d.get('num_dbs'), 0) > 0:
			args_d['db_num_keys']   = coalesce(args_d.get('db_num_keys'), 500000000 )
			args_d['db_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_{x}' for x in range(0, args_d['num_dbs']) ])
			args_d['db_benchmark']  = coalesce(args_d.get('db_benchmark'), 'readwhilewriting' )
			args_d['db_threads']    = coalesce(args_d.get('db_threads'), 1 )
			args_d['db_cache_size'] = coalesce(args_d.get('db_cache_size'), 536870912 )

		if coalesce(args_d.get('num_ydbs'), 0) > 0:
			args_d['ydb_num_keys'] = coalesce(args_d.get('ydb_num_keys'), 50000000 )
			args_d['ydb_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_ycsb_{x}' for x in range(0, args_d['num_ydbs']) ])
			args_d['ydb_threads']  = coalesce(args_d.get('ydb_threads'), 1 )
			args_d['ydb_workload'] = coalesce(args_d.get('ydb_workload'), 'workloadb' )
			args_d['ydb_sleep']    = coalesce(args_d.get('ydb_sleep'), 0 )
			
		if coalesce(args_d.get('num_at'), 0) > 0:
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
		docker_params.append(f'--user={os.getuid()}')
		args_d['docker_params'] = ' '.join(docker_params)

		return args_d
		
	def run(self, args_d=None):
		log.info('')
		log.info('==========================================')
		args_d = coalesce( args_d, self.get_args_d() )
		
		cmd = 'build/rocksdb_test \\\n'
		cmd += f'	--log_level="info"  \\\n'
		cmd += f'	--stats_interval=5  \\\n'
		
		output_path = coalesce(args_d.get('output_path'), '')
		if output_path != '':
			output_path = f'{test_dir(output_path)}/'

		def_p_func = lambda k, v: f'	--{k}="{args_d[k]}" \\\n'
		if self.exp_params.get('ydb_workload') is not None:
			self.exp_params['ydb_workload']['p_func'] = lambda k, v: f'	--{k}="/workspace/YCSB/workloads/{args_d[k]}" \\\n'
		if self.exp_params.get('params') is not None:
			self.exp_params['params']['p_func'] = lambda k, v: f'	{args_d[k]}'
		if self.exp_params.get('output') is not None:
			self.exp_params['output']['p_func'] = lambda k, v: f' > "{output_path}{args_d[k]}"'
		
		for k, v in self.exp_params.items():
			if args_d.get(k) is not None:
				log.info(f'{k:<20} = {args_d.get(k)}')
			if args_d.get(k) is not None:
				p_func = coalesce(v.get('p_func'), def_p_func)
				cmd += p_func(k, v)
				
		self.before_run(args_d)
		
		if args_d.get('before_run_cmd') is not None:
			command(args_d.get('before_run_cmd'), cmd_args=args_d)

		args_d['exit_code'] = command(cmd, raise_exception=False)

		if args_d.get('after_run_cmd') is not None:
			command(args_d.get('after_run_cmd'), cmd_args=args_d)

	def before_run(self, args_d):
		self.restore_dbs(args_d)
		self.test_paths(args_d)

	def test_paths(self, args_d):
		if coalesce(args_d.get('num_ydbs'), 0) > 0:
			for db in args_d['ydb_path'].split('#'):
				test_path(f'{db}/CURRENT')
		if coalesce(args_d.get('num_dbs'), 0) > 0:
			for db in args_d['db_path'].split('#'):
				test_path(f'{db}/CURRENT')
	
	def restore_dbs(self, args_d):
		def rm_old_dbs():
			log.info('Removing old database directores before restoring backup...')
			command(f'rm -fr {args_d["data_path"]}/rocksdb_*')
			
		if coalesce(args_d.get('backup_ycsb'), '') != '' and coalesce(args_d.get('num_ydbs'), 0) > 0:
			log.info(f"Using database backup file: {args_d['backup_ycsb']}")
			tarfile = test_path(args_d['backup_ycsb'])
			rm_old_dbs()
			for db in args_d['ydb_path'].split('#'):
				log.info(f'Restoring backup on directory {db}..')
				command(f'mkdir "{db}"')
				command(f'tar -xf "{tarfile}" -C "{db}"')
				
		if coalesce(args_d.get('backup_dbbench'), '') != '' and coalesce(args_d.get('num_dbs'), 0) > 0:
			log.info(f"Using database backup file: {args_d['backup_dbbench']}")
			tarfile = test_path(args_d.get('backup_dbbench'))
			rm_old_dbs()
			for db in args_d['db_path'].split('#'):
				log.info(f'Restoring backup on directory {db}..')
				command(f'mkdir "{db}"')
				command(f'tar -xf "{tarfile}" -C "{db}"')
			
		
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
class Exp_create_ycsb (GenericExperiment):
	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		global experiment_list
		experiment_list['create_ycsb'] = cls
		
		parser = subparsers.add_parser('create_ycsb', help='create the database used for YCSB benchmark')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period',
		                 'rocksdb_config_file', 'num_ydbs', 'ydb_num_keys', 'ydb_path',
		                 'ydb_threads', 'ydb_workload', 'params', 'output'])
		
		cls.exp_params['rocksdb_config_file']['default'] = '/workspace/rocksdb_test/files/rocksdb-6.8-db_bench.options'
		cls.exp_params['duration']['default']     = 60
		cls.exp_params['num_ydbs']['default']     = 1
		cls.exp_params['ydb_threads']['default']  = 4
		cls.exp_params['ydb_workload']['default'] = 'workloada'
		cls.exp_params['ydb_create'] = {'type':str,  'default':'true', 'help':None }
		cls.exp_params.move_to_end('params')
		cls.exp_params.move_to_end('output')

		cls.set_args(parser, load_args)
	
	def run(self):
		args_d = self.get_args_d()
		args_d['output'] = f'ycsb_create.out'

		backup_file = args_d.get('backup_ycsb')
		if os.path.exists(backup_file):
			raise Exception(f'YCSB database backup file already exists: {backup_file}')

		super(self.__class__, self).run(args_d)

		if coalesce(args_d.get('exit_code'), 0) != 0:
			raise Exception(f"Database creation returned error code {args_d.get('exit_code')}")

		self.test_paths(args_d)

		if coalesce(args_d.get('exit_code'), 0) == 0 and coalesce(backup_file, '') != '':
			db = args_d['ydb_path'].split('#')[0]
			command(f'tar -C "{db}" -cf {backup_file} .')

	def before_run(self, args_d):
		log.info('Removing old database directores ...')
		command(f'rm -fr {args_d["data_path"]}/rocksdb_*')
		
		for db in args_d['ydb_path'].split('#'):
			log.info(f'Creating database directory {db} ...')
			command(f'mkdir "{db}"')

#=============================================================================
class Exp_ycsb (GenericExperiment):
	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		global experiment_list
		experiment_list['ycsb'] = cls
		
		parser = subparsers.add_parser('ycsb', help='YCSB benchmark')

		parser.add_argument('--ydb_workload_list', type=str, default=coalesce(load_args.get('ydb_workload_list'), 'workloadb workloada'),
			help='list of YCSB workloads (space separated)')
		
		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period',
		                 'num_ydbs', 'ydb_num_keys', 'ydb_path', 'ydb_threads',
		                 'ydb_workload', 'perfmon', 'perfmon_port', 'params', 'output'])
		
		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_ydbs']['default']     = 1
		cls.exp_params['ydb_threads']['default']  = 5

		cls.set_args(parser, load_args)
	
	def run(self):
		args_d = self.get_args_d()
		
		for ydb_workload in args_d['ydb_workload_list'].split(' '):
			args_d['ydb_workload'] = ydb_workload
			args_d['output'] = f'ycsb_{ydb_workload}.out'
			
			super(self.__class__, self).run(args_d)

#=============================================================================
class Exp_ycsb_at3 (GenericExperiment):
	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		global experiment_list
		experiment_list['ycsb_at3'] = cls
		
		parser = subparsers.add_parser('ycsb_at3', help='1x YCSB benchmark + 4x access_time3')

		parser.add_argument('--ydb_workload_list', type=str, default=coalesce(load_args.get('ydb_workload_list'), 'workloadb workloada'),
			help='list of YCSB workloads (space separated)')
		parser.add_argument('--at_block_size_list', type=str, default=coalesce(load_args.get('at_block_size_list'), '512 4'),
			help='list of access_time3\'s block sizes (space separated)')
		parser.add_argument('--at_interval', type=int, default=coalesce(load_args.get('at_interval'), 2),
			help='interval between changes in the access_time3\' access pattern')
		
		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period', 'num_ydbs',
		                 'ydb_num_keys', 'ydb_path', 'ydb_threads', 'ydb_workload', 'ydb_sleep',
		                 'num_at', 'at_dir', 'at_file', 'at_block_size', 'at_params', 'at_script',
		                 'perfmon', 'perfmon_port', 'params', 'output'])

		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_ydbs']['default']     = 1
		cls.exp_params['ydb_threads']['default']  = 5
		cls.exp_params['num_at']['default']       = 4
		cls.exp_params['at_params']['default']    = '--flush_blocks=0 --random_ratio=0.5 --wait --direct_io'

		cls.set_args(parser, load_args)
	
	def run(self):
		args_d = self.get_args_d()
		
		args_d['at_script'] = self.get_at3_script(int(args_d['warm_period'])+10, int(args_d['num_at']), int(args_d['at_interval']))
		
		for at_bs in args_d['at_block_size_list'].split(' '):
			args_d['at_block_size'] = at_bs
			for ydb_workload in args_d['ydb_workload_list'].split(' '):
				args_d['ydb_workload'] = ydb_workload
				args_d['output'] = f'ycsb_{ydb_workload},at3_bs{at_bs}_directio.out'
				
				super(self.__class__, self).run(args_d)

#=============================================================================
def command_output(cmd, raise_exception=True):
	log.debug(f'Executing command: {cmd}')
	err, out = subprocess.getstatusoutput(cmd)
	if err != 0:
		msg = f'erro {err} from command "{cmd}"'
		if raise_exception:
			raise Exception(msg)
		else:
			log.error(msg)
	return out

def command(cmd, raise_exception=True, cmd_args=None):
	if cmd_args is not None:
		cmd = cmd.format(**cmd_args)

	if args.confirm_cmd:
		sys.stdout.write(f'Execute command?\n\t{cmd}\n')
		while True:
			sys.stdout.write(f'y (yes) / n (no) /a (always): ')
			sys.stdout.flush()
			l = sys.stdin.readline().strip().lower()
			if l in ['a', 'always']:
				args.confirm_cmd = False
				break
			elif l in ['n', 'no']:
				return
			elif l in ['y', 'yes']:
				break
			sys.stdout.write(f'invalid option\n')
	
	log.debug(f'Executing command: {cmd}')
	with subprocess.Popen(cmd, shell=True) as p:
		exit_code = p.wait()
	log.debug(f'Exit code: {exit_code}')
	
	if exit_code != 0:
		msg = f'Exit code {exit_code} from command "{cmd}"'
		if raise_exception:
			raise Exception(msg)
		else:
			log.error(msg)
	return exit_code

def test_dir(d):
	if not os.path.isdir(d):
		raise Exception(f'directory "{d}" does not exist')
	return d

def test_path(f):
	if not os.path.exists(f):
		raise Exception(f'path "{f}" does not exist')
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
		args_d = args_to_dir(args)
		for k, v in args_d.items():
			v2 = f'"{v}"' if v is not None else ''
			log.info(f'Argument {k:<20} = {v2}')

#=============================================================================
def signal_handler(signame, signumber, stack):
	try:
		log.warning("signal {} received".format(signame))
		for p in psutil.Process().children(recursive=True):
			try:
				log.warning(f'Child process {p.pid} is still running. Kill it.')
				p.terminate()
			except Exception as e:
				sys.stderr.write(f'signal_handler exception1: {str(e)}\n')
	except Exception as e:
		sys.stderr.write(f'signal_handler exception2: {str(e)}\n')
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
