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
import re

#=============================================================================
import logging
log = logging.getLogger('rocksdb_test_helper')
log.setLevel(logging.INFO)

#=============================================================================
class ExperimentList (collections.OrderedDict):
	def register(self, cls):
		self[cls.exp_name] = cls
experiment_list = ExperimentList()

#=============================================================================
class ArgsWrapper: # single global instance "args"
	def get_args(self):
		preparser = argparse.ArgumentParser("rocksdb_test helper", add_help=False)
		preparser.add_argument('-l', '--log_level', type=str,
			default='INFO', choices=[ 'debug', 'DEBUG', 'info', 'INFO' ],
			help='Log level.')
		preparser.add_argument('--load_args', type=str,
			default=None,
			help='Load arguments from file (JSON).')
		preparser.add_argument('--save_args', type=str,
			default=None,
			help='Save arguments to a file (JSON).')
		preargs, remainargv = preparser.parse_known_args()

		log_h = logging.StreamHandler()
		#log_h.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s: %(message)s'))
		log_h.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
		log.addHandler(log_h)
		log.setLevel(getattr(logging, preargs.log_level.upper()))

		log.debug(f'Preargs: {str(preargs)}')

		argcheck_path(preargs, 'load_args', required=False, absolute=False,  type='file')
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
			help='Log level.')
		parser.add_argument('--load_args', type=str,
			default=None,
			help='Load arguments from file (JSON).')
		parser.add_argument('--save_args', type=str,
			default=None,
			help='Save arguments to a file (JSON).')

		parser.add_argument('--data_path', type=str, required=True,
			default=load_args.get('data_path'),
			help='Directory used to store both database and access_time3 files.')
		parser.add_argument('--output_path', type=str,
			default=coalesce(load_args.get('output_path'), '.'),
			help='Output directory of the experiments (default=./).')

		parser.add_argument('--rocksdb_test_path', type=str,
			default=load_args.get('rocksdb_test_path'),
			help='Directory of rocksdb_test (optional).')
		parser.add_argument('--rocksdb_path', type=str,
			default=load_args.get('rocksdb_path'),
			help='Directory of rocksdb (optional).')
		parser.add_argument('--ycsb_path', type=str,
			default=load_args.get('ycsb_path'),
			help='Directory of YCSB (optional).')

		parser.add_argument('--confirm_cmd', type=str, nargs='?', const=True,
			default=coalesce(load_args.get('confirm_cmd'), False),
			help='Confirm before each command execution.')

		parser.add_argument('--test', type=str,
			default='',
			help='Test routines.')

		subparsers = parser.add_subparsers(dest='experiment', title='Experiments available', description='Use "./rocksdb_test_helper <experiment_name> -h" to list the arguments of each experiment.')
		for eclass in experiment_list.values():
			eclass.register_subcommand(subparsers, load_args)

		args = parser.parse_args(remainargv)
		log.debug(f'Arguments: {str(args)}')

		argcheck_bool(args, 'confirm_cmd', required=True)

		argcheck_path(args, 'data_path',         required=True,  absolute=True,  type='dir')
		argcheck_path(args, 'output_path',       required=True,  absolute=False, type='dir')
		argcheck_path(args, 'rocksdb_test_path', required=False, absolute=True,  type='dir')
		argcheck_path(args, 'rocksdb_path',      required=False, absolute=True,  type='dir')
		argcheck_path(args, 'ycsb_path',         required=False, absolute=True,  type='dir')

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

def argcheck_bool(args, arg_name, required=False):
	log.debug(f'argcheck_bool: --{arg_name}, required={required}')
	value, setf = value_setf(args, arg_name)
	log.debug(f'argcheck_bool: --{arg_name}="{value}"')
	if value is None:
		if required:  raise ValueError(f'argument --{arg_name} is required')
		else:         return
	elif isinstance(value, bool):
		return
	elif isinstance(value, str) and value.lower().strip() in ['true', 't', 'yes', 'y', '1']:
		setf(True)
	elif isinstance(value, str) and value.lower().strip() in ['false', 'f', 'no', 'n', '0']:
		setf(False)
	else:
		raise ValueError(f'invalid value for boolean argument --{arg_name}="{value}"')

def argcheck_path(args, arg_name, required=False, absolute=False, type='file'):
	log.debug(f'argcheck_path: --{arg_name}, type="{type}", required={required}, absolute={absolute}')
	check_f = getattr(os.path, f'is{type}')

	value, setf = value_setf(args, arg_name)
	log.debug(f'argcheck_path: --{arg_name}="{value}"')

	if value is None:
		if required:  raise ValueError(f'argument --{arg_name} is required')
		else:         return
	elif isinstance(value, str) and check_f(value):
		if absolute:
			setf( os.path.abspath(value) )
	else:
		raise ValueError(f'argument --{arg_name}="{value}" is not a directory')

args = ArgsWrapper()

#=============================================================================
class GenericExperiment:
	exp_name = 'generic'

	exp_params = collections.OrderedDict([
		#Name,                   Type       Def   Help
		('docker_image',          {'type':str,  'default':None,        'help':'Docker image (optional). Use rocksdb_test\'s default if not informed.' }),
		('docker_params',         {'type':str,  'default':None,        'help':'Additional docker arguments.' }),
		('duration',              {'type':int,  'default':None,        'help':'Duration of the experiment (in minutes).' }),
		('warm_period',           {'type':int,  'default':None,        'help':'Warm period (in minutes).' }),
		('rocksdb_config_file',   {'type':str,  'default':None,        'help':'Rocksdb config file used to create the database.' }),
		('num_dbs',               {'type':int,  'default':0,           'help':'Number of db_bench instances.' }),
		('db_num_keys',           {'type':str,  'default':'10000',     'help':'Number of keys used by db_bench (default=10000). Use a greater value for realistic experiments (e.g., 500000000).' }),
		('db_path',               {'type':str,  'default':None,        'help':'One database directory for each instance of db_bench separated by "#". This argument is configured automatically (<DATA_PATH>/rocksdb_0#<DATA_PATH>/rocksdb_1#...).' }),
		('db_benchmark',          {'type':str,  'default':'readwhilewriting', 'help':'db_bench workload (default=readwhilewriting).' }),
		('db_threads',            {'type':str,  'default':'9',         'help':'Number of threads used by the db_bench workload (default=9).' }),
		('db_cache_size',         {'type':str,  'default':'536870912', 'help':'Cache size used by db_bench (default=512MiB).' }),
		('num_ydbs',              {'type':int,  'default':0,           'help':'Number of YCSB instances.' }),
		('ydb_num_keys',          {'type':str,  'default':'10000',     'help':'Number of keys used by YCSB (default=10000). Use a greater value for realistic experiments (e.g., 50000000).' }),
		('ydb_path',              {'type':str,  'default':None,        'help':'One database directory for each instance of YCSB separated by "#". This argument is configured automatically (<DATA_PATH>/rocksdb_ycsb_0#<DATA_PATH>/rocksdb_ycsb_1#...).' }),
		('ydb_threads',           {'type':str,  'default':'5',         'help':'Number of threads used by YCSB workload (default=5).' }),
		('ydb_workload',          {'type':str,  'default':'workloadb', 'help':'YCSB workload (default=workloadb).' }),
		('ydb_sleep',             {'type':str,  'default':'0',         'help':'Sleep before executing each YCSB instance (in minutes, separated by "#").' }),
		('num_at',                {'type':int,  'default':0,           'help':'Number of access_time3 instances.' }),
		('at_dir',                {'type':str,  'default':None,        'help':'Directory containing the files used by the access_time3 instances. By default, this argument is configured automatically (<DATA_PATH>/tmp).' }),
		('at_file',               {'type':str,  'default':None,        'help':'Files used by the access_time3 instances (separated by #). Also configured automatically.' }),
		('at_block_size',         {'type':str,  'default':'512',       'help':'Block size used by the access_time3 instances (default=512).' }),
		('at_params',             {'type':str,  'default':None,        'help':'Extra access_time3 arguments, if necessary.' }),
		('at_script',             {'type':str,  'default':None,        'help':'Access_time3 script (separated by "#"). Generated automatically by experiments ycsb_at3 and dbbench_at3.' }),
		('perfmon',               {'type':str,  'default':None,        'help':'Connect to performancemonitor.' }),
		('perfmon_port',          {'type':int,  'default':None,        'help':'performancemonitor port' }),
		('params',                {'type':str,  'default':None,        'help':'Extra rocksdb_test arguments.' }),
		('output',                {'type':str,  'default':None,        'help':'Output file of each experiment (<OUTPUT_DIR>/<OUTPUT>).' }),
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
			help='Restore the database backup used by the db_bench instances from this .tar file (don\'t use subdir).')
		parser.add_argument('--backup_ycsb', type=str, default=load_args.get('backup_ycsb'),
			help='Restore the database backup used by the YCSB instances from this .tar file (don\'t use subdirs).')

		parser.add_argument('--before_run_cmd', type=str, default=load_args.get('before_run_cmd'),
			help='Execute this command before running the rocksdb_test.')
		parser.add_argument('--after_run_cmd', type=str, default=load_args.get('after_run_cmd'),
			help='Execute this command after running the rocksdb_test.')

		for k, v in cls.exp_params.items():
			parser.add_argument(f'--{k}', type=v['type'], default=coalesce(load_args.get(k), v.get('default')),
				help=v.get('help'))

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
			args_d['db_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_{x}' for x in range(0, args_d['num_dbs']) ])

		if coalesce(args_d.get('num_ydbs'), 0) > 0:
			args_d['ydb_path'] = '#'.join([ f'{args_d["data_path"]}/rocksdb_ycsb_{x}' for x in range(0, args_d['num_ydbs']) ])

		if coalesce(args_d.get('num_at'), 0) > 0:
			args_d['at_dir']        = coalesce(args_d.get('at_dir'), f'{args_d["data_path"]}/tmp' )
			argcheck_path(args_d, 'at_dir', required=True, absolute=True, type='dir')
			args_d['at_file'] = '#'.join([ str(x) for x in range(0, args_d['num_at']) ])

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

		def_p_func = lambda k, v: f'	--{k}="{args_d[k]}" \\\n'
		if self.exp_params.get('ydb_workload') is not None:
			self.exp_params['ydb_workload']['p_func'] = lambda k, v: f'	--{k}="/workspace/YCSB/workloads/{args_d[k]}" \\\n'
		if self.exp_params.get('params') is not None:
			self.exp_params['params']['p_func'] = lambda k, v: f'	{args_d[k]}'
		if self.exp_params.get('output') is not None:
			self.exp_params['output']['p_func'] = lambda k, v: f' > "{os.path.join(output_path, args_d[k])}"'

		for k, v in self.exp_params.items():
			if args_d.get(k) is not None:
				log.info(f'{k:<20} = {args_d.get(k)}')
			if args_d.get(k) is not None:
				p_func = coalesce(v.get('p_func'), def_p_func)
				cmd += p_func(k, v)

		self.before_run(args_d)

		if args_d.get('before_run_cmd') is not None:
			command(args_d.get('before_run_cmd'), cmd_args=args_d)

		log.info(f'Executing rocksdb_test experiment {args_d.get("experiment")} ...')
		args_d['exit_code'] = command(cmd, raise_exception=False)
		log.info(f'Experiment {args_d.get("experiment")} finished.')

		if coalesce(args_d.get('exit_code'), 0) != 0:
			log.error(f"rocksdb_test returned error code {args_d.get('exit_code')}. Check output file \"{args_d.get('output')}\"")

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

	def remove_old_dbs(self, args_d):
		data_path = args_d["data_path"]
		log.info(f'Removing old database directores from data_path ({data_path}) ...')
		rm_entries = []
		for p in os.listdir(data_path):
			entry = os.path.join(data_path, p)
			log.debug(f'remove_old_dbs: entry = {entry}')
			if os.path.isdir(entry):
				r = re.findall(r'(rocksdb_(ycsb_){0,1}[0-9]+)', entry)
				log.debug(f'remove_old_dbs: r = {r}')
				if len(r) > 0:
					rm_entries.append(entry)
					log.info(f'\t{entry}')
		log.debug(f'remove_old_dbs: rm_entries = {rm_entries}')
		rm_entries = [f"'{x}'" for x in rm_entries]
		if len(rm_entries) > 0:
			command(f'rm -fr {" ".join(rm_entries)}')

	def restore_dbs(self, args_d):
		if coalesce(args_d.get('backup_ycsb'), '') != '' and coalesce(args_d.get('num_ydbs'), 0) > 0:
			argcheck_path(args_d, 'backup_ycsb',    required=True, absolute=False, type='file')
			log.info(f"Using database backup file: {args_d['backup_ycsb']}")
			tarfile = args_d['backup_ycsb']
			self.remove_old_dbs(args_d)
			for db in args_d['ydb_path'].split('#'):
				log.info(f'Restoring backup on directory {db}..')
				if os.path.exists(db):
					raise Exception(f'cannot restore backup on an existing path: {db}')
				command(f'mkdir -p "{db}"')
				command(f'tar -xf "{tarfile}" -C "{db}"')

		if coalesce(args_d.get('backup_dbbench'), '') != '' and coalesce(args_d.get('num_dbs'), 0) > 0:
			argcheck_path(args_d, 'backup_dbbench', required=True, absolute=False, type='file')
			log.info(f"Using database backup file: {args_d['backup_dbbench']}")
			tarfile = args_d['backup_dbbench']
			self.remove_old_dbs(args_d)
			for db in args_d['db_path'].split('#'):
				log.info(f'Restoring backup on directory {db}..')
				if os.path.exists(db):
					raise Exception(f'cannot restore backup on an existing path: {db}')
				command(f'mkdir -p "{db}"')
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
	exp_name = 'create_ycsb'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help="Creates the database used by the experiments with YCSB benchmark.", description='This subcommand creates the database used by the experiments with YCSB benchmark. After database creation, it executes <YDB_WORKLOAD> for <DURATION> minutes and then creates a backup into the file <BACKUP_YCSB>, if informed.')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period', 'rocksdb_config_file',
		                 'num_ydbs', 'ydb_num_keys', 'ydb_path', 'ydb_threads', 'ydb_workload',
		                 'params', 'output'])

		cls.exp_params['rocksdb_config_file']['default'] = '/workspace/rocksdb_test/files/rocksdb-6.8-db_bench.options'
		cls.exp_params['duration']['default']     = 60
		cls.exp_params['warm_period']['default']  = 0
		cls.exp_params['num_ydbs']['default']     = 1
		cls.exp_params['ydb_threads']['default']  = 4
		cls.exp_params['ydb_workload']['default'] = 'workloada'
		cls.exp_params['ydb_create'] = {'type':str,  'default':'true', 'help':None }
		cls.exp_params.move_to_end('params')
		cls.exp_params.move_to_end('output')

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()
		args_d['num_ydbs'] = 1
		args_d['output'] = f'ycsb_create.out'

		backup_file = coalesce(args_d.get('backup_ycsb'), '').strip()
		if backup_file != '' and os.path.exists(backup_file):
			raise Exception(f'YCSB database backup file already exists: {backup_file}')

		super(self.__class__, self).run(args_d)

		if coalesce(args_d.get('exit_code'), 0) != 0:
			raise Exception(f"Database creation returned error code {args_d.get('exit_code')}. Check output file \"{args_d.get('output')}\"")

		self.test_paths(args_d)

		if coalesce(args_d.get('exit_code'), 0) == 0 and coalesce(backup_file, '') != '':
			log.info(f'Creating backup file "{backup_file}" ...')
			db = args_d['ydb_path'].split('#')[0]
			command(f'tar -C "{db}" -cf {backup_file} .')

	def before_run(self, args_d):
		db = args_d['ydb_path'].split('#')[0]

		log.info('Removing old database directory ...')
		command(f'rm -fr "{db}"')

		log.info(f'Creating database directory {db} ...')
		command(f'mkdir -p "{db}"')

experiment_list.register( Exp_create_ycsb )

#=============================================================================
class Exp_ycsb (GenericExperiment):
	exp_name = 'ycsb'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help='Executes the YCSB benchmark.', description='This experiment executes <NUM_YDBS> simultaneous instances of the YCSB benchmark for <DURATION> minutes, including <WARM_PERIOD> minutes of warmup. If <BACKUP_YCSB> is informed, it removes the old database directories and restores the backup before the experiment.')

		parser.add_argument('--ydb_workload_list', type=str, default=load_args.get('ydb_workload_list'),
			help='If necessary to execute this experiment with more than one workload, specify the list here (space separated).')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period',
		                 'num_ydbs', 'ydb_num_keys', 'ydb_path', 'ydb_threads',
		                 'ydb_workload', 'perfmon', 'perfmon_port', 'params', 'output'])

		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_ydbs']['default']     = 1

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()

		if coalesce(args_d.get('ydb_workload_list'), '').strip() == '':
			args_d['ydb_workload_list'] = args_d.get('ydb_workload')

		for ydb_workload in args_d['ydb_workload_list'].split(' '):
			args_d['ydb_workload'] = ydb_workload
			args_d['output'] = f'ycsb_{ydb_workload}.out'

			super(self.__class__, self).run(args_d)

experiment_list.register( Exp_ycsb )

#=============================================================================
class Exp_ycsb_at3 (GenericExperiment):
	exp_name = 'ycsb_at3'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help='YCSB benchmark + access_time3', description='This experiment executes, simultaneously, <NUM_YDBS> YCSB and <NUM_AT> access_time3 instances for <DURATION> minutes, including <WARM_PERIOD> minutes of warmup. If <BACKUP_YCSB> is informed, it removes the old database directories and restores the backup before the experiment.')

		parser.add_argument('--ydb_workload_list', type=str, default=load_args.get('ydb_workload_list'),
			help='If necessary to execute this experiment with more than one workload, specify the list here (space separated).')
		parser.add_argument('--at_block_size_list', type=str, default=load_args.get('at_block_size_list'),
			help='If necessary to execute this experiment with more than one access_time3\'s block size, specify the list here (space separated).')
		parser.add_argument('--at_interval', type=int, default=coalesce(load_args.get('at_interval'), 2),
			help='Interval between changes in the access pattern of the access_time3 instances.')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period', 'num_ydbs',
		                 'ydb_num_keys', 'ydb_path', 'ydb_threads', 'ydb_workload', 'ydb_sleep',
		                 'num_at', 'at_dir', 'at_file', 'at_block_size', 'at_params', 'at_script',
		                 'perfmon', 'perfmon_port', 'params', 'output'])

		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_ydbs']['default']     = 1
		cls.exp_params['num_at']['default']       = 4
		cls.exp_params['at_params']['default']    = '--flush_blocks=0 --random_ratio=0.5 --wait --direct_io'

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()

		args_d['at_script'] = self.get_at3_script(int(args_d['warm_period'])+10, int(args_d['num_at']), int(args_d['at_interval']))

		if coalesce(args_d.get('ydb_workload_list'), '').strip() == '':
			args_d['ydb_workload_list'] = args_d.get('ydb_workload')
		if coalesce(args_d.get('at_block_size_list'), '').strip() == '':
			args_d['at_block_size_list'] = args_d.get('at_block_size')

		for at_bs in args_d['at_block_size_list'].split(' '):
			args_d['at_block_size'] = at_bs
			for ydb_workload in args_d['ydb_workload_list'].split(' '):
				args_d['ydb_workload'] = ydb_workload
				args_d['output'] = f'ycsb_{ydb_workload},at3_bs{at_bs}_directio.out'
				super(self.__class__, self).run(args_d)

experiment_list.register( Exp_ycsb_at3 )

#=============================================================================
class Exp_create_dbbench (GenericExperiment):
	exp_name = 'create_dbbench'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help="Creates the database used by the experiments with db_bench.", description='This subcommand creates the database used by the experiments with db_bench. After database creation, it executes <DB_BENCHMARK> for <DURATION> minutes and then creates a backup into the file <BACKUP_DBBENCH>, if informed.')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period', 'rocksdb_config_file',
		                 'num_dbs', 'db_num_keys', 'db_path', 'db_benchmark', 'db_threads', 'db_cache_size',
		                 'params', 'output'])

		cls.exp_params['rocksdb_config_file']['default'] = '/workspace/rocksdb_test/files/rocksdb-6.8-db_bench.options'
		cls.exp_params['duration']['default']     = 60
		cls.exp_params['warm_period']['default']  = 0
		cls.exp_params['num_dbs']['default']      = 1
		cls.exp_params['db_threads']['default']   = 9
		cls.exp_params['db_create'] = {'type':str,  'default':'true', 'help':None }
		cls.exp_params.move_to_end('params')
		cls.exp_params.move_to_end('output')

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()
		args_d['num_dbs'] = 1
		args_d['output'] = f'dbbench_create.out'

		backup_file = coalesce(args_d.get('backup_dbbench'), '').strip()
		if backup_file != '' and os.path.exists(backup_file):
			raise Exception(f'db_bench database backup file already exists: {backup_file}')

		super(self.__class__, self).run(args_d)

		if coalesce(args_d.get('exit_code'), 0) != 0:
			raise Exception(f"Database creation returned error code {args_d.get('exit_code')}. Check output file \"{args_d.get('output')}\"")

		self.test_paths(args_d)

		if coalesce(args_d.get('exit_code'), 0) == 0 and backup_file != '':
			log.info(f'Creating backup file "{backup_file}" ...')
			db = args_d['db_path'].split('#')[0]
			command(f'tar -C "{db}" -cf {backup_file} .')

	def before_run(self, args_d):
		db = args_d['db_path'].split('#')[0]

		log.info('Removing old database directory ...')
		command(f'rm -fr "{db}"')

		log.info(f'Creating database directory {db} ...')
		command(f'mkdir -p "{db}"')

experiment_list.register( Exp_create_dbbench )

#=============================================================================
class Exp_dbbench (GenericExperiment):
	exp_name = 'dbbench'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help='Executes the db_bench benchmark.', description='This experiment executes <NUM_DBS> simultaneous instances of db_bench for <DURATION> minutes, including <WARM_PERIOD> minutes of warmup. If <BACKUP_DBBENCH> is informed, it also removes the old database directories and restores the backup before the experiment.')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period',
		                 'num_dbs', 'db_num_keys', 'db_path', 'db_benchmark', 'db_threads', 'db_cache_size',
		                 'perfmon', 'perfmon_port', 'params', 'output'])

		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_dbs']['default']      = 1

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()

		args_d['output'] = f'dbbench_{args_d.get("db_benchmark")}.out'
		super(self.__class__, self).run(args_d)

experiment_list.register( Exp_dbbench )

#=============================================================================
class Exp_dbbench_at3 (GenericExperiment):
	exp_name = 'dbbench_at3'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help='db_bench + access_time3', description='This experiment executes, simultaneously, <NUM_DBS> db_bench and <NUM_AT> access_time3 instances for <DURATION> minutes, including <WARM_PERIOD> minutes of warmup. If <BACKUP_DBBENCH> is informed, it removes the old database directories and restores the backup before the experiment.')

		parser.add_argument('--at_block_size_list', type=str, default=load_args.get('at_block_size_list'),
			help='If necessary to execute this experiment with more than one access_time3\'s block size, specify the list here (space separated).')
		parser.add_argument('--at_interval', type=int, default=coalesce(load_args.get('at_interval'), 2),
			help='Interval between changes in the access pattern of the access_time3 instances.')

		cls.filter_args(['docker_image', 'docker_params', 'duration', 'warm_period',
		                 'num_dbs', 'db_num_keys', 'db_path', 'db_benchmark', 'db_threads', 'db_cache_size',
		                 'num_at', 'at_dir', 'at_file', 'at_block_size', 'at_params', 'at_script',
						 'perfmon', 'perfmon_port', 'params', 'output'])

		cls.exp_params['duration']['default']     = 90
		cls.exp_params['warm_period']['default']  = 30
		cls.exp_params['num_dbs']['default']      = 1
		cls.exp_params['num_at']['default']       = 4
		cls.exp_params['at_params']['default']    = '--flush_blocks=0 --random_ratio=0.5 --wait --direct_io'

		cls.set_args(parser, load_args)

	def run(self):
		args_d = self.get_args_d()

		if coalesce(args_d.get('at_block_size_list'), '').strip() == '':
			args_d['at_block_size_list'] = args_d.get('at_block_size')

		for at_bs in args_d['at_block_size_list'].split(' '):
			args_d['at_block_size'] = at_bs
			args_d['output'] = f'dbbench_{args_d.get("db_benchmark")},at3_bs{at_bs}_directio.out'
			super(self.__class__, self).run(args_d)

experiment_list.register( Exp_dbbench_at3 )

#=============================================================================
class Exp_create_at3 (GenericExperiment):
	exp_name = 'create_at3'

	@classmethod
	def register_subcommand(cls, subparsers, load_args):
		parser = subparsers.add_parser(cls.exp_name, help='Creates the data files used by the access_time3 instances.', description='This subcommand creates the data files used by the <NUM_AT> access_time3 instances. These files will be stored in <AT_DIR> and have <AT_FILE_SIZE> MiB each.')

		cls.filter_args(['docker_image', 'docker_params',
		                 'num_at', 'at_file', 'at_dir', 'at_params', 'output'])

		cls.exp_params['num_at']['default'] = 4

		cls.set_args(parser, load_args)

	@classmethod
	def set_args(cls, parser, load_args):
		parser.add_argument('--before_run_cmd', type=str, default=load_args.get('before_run_cmd'),
			help='Execute this command before creating these files.')
		parser.add_argument('--after_run_cmd', type=str, default=load_args.get('after_run_cmd'),
			help='Execute this command after creating these files.')

		parser.add_argument('--at_file_size', type=int, default=coalesce(load_args.get('at_file_size'), 10000),
			help='file size used by each instance of access_time3')

		for k, v in cls.exp_params.items():
			parser.add_argument(f'--{k}', type=v['type'], default=coalesce(load_args.get(k), v.get('default')),
				help=v.get('help'))

	def process_args_d(self, args_d):
		args_d = super(self.__class__, self).process_args_d(args_d)
		args_d['at_params'] = coalesce(args_d.get('at_params'), '') + \
			f' --create_file --filesize={args_d.get("at_file_size")} --duration=1'
		args_d['output'] = f'at3_create.out'
		return args_d

	def run(self):
		#build/access_time3 --create_file --filesize=10000 --filename=/workdata/0 --duration=1
		args_d = self.get_args_d()

		super(self.__class__, self).run(args_d)

		if coalesce(args_d.get('exit_code'), 0) != 0:
			raise Exception(f"File creation returned error code {args_d.get('exit_code')}. Check output file \"{args_d.get('output')}\"")

	def before_run(self, args_d):
		pass

experiment_list.register( Exp_create_at3 )

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

def value_setf(args, arg_name):
	try:
		value = getattr(args, arg_name)
		setf  = lambda val: setattr(args, arg_name, val)
	except:
		try:
			value = args[arg_name]
			setf = lambda val: args.__setitem__(arg_name, val)
		except:
			raise KeyError(f'failed to get the value of attribute {arg_name}')
	return value, setf

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
def main():
	exp_class = experiment_list.get(args.experiment)
	if exp_class is not None:
		exp_class().run()

	return 0

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
