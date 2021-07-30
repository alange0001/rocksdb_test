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
import re
import sys
import argparse
import traceback


# =============================================================================
import logging
log = logging.getLogger('rocksdb_config_gen')
log.setLevel(logging.INFO)


# =============================================================================
class ArgsWrapper:  # single global instance of "args"
	def get_args(self):
		parser = argparse.ArgumentParser(
			description="rocksdb config generator")
		parser.add_argument('-l', '--log_level', type=str,
			default='INFO', choices=[ 'debug', 'DEBUG', 'info', 'INFO' ],
			help='Log level.')
		parser.add_argument('--output', type=str,
			default='',
			help='Output config file.')
		parser.add_argument('--template', type=str,
			default='08',
			help='Template number.')

		args, remainargv = parser.parse_known_args()

		log_h = logging.StreamHandler()
		# log_h.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s: %(message)s'))
		log_h.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
		log.addHandler(log_h)
		log.setLevel(getattr(logging, args.log_level.upper()))

		log.debug(f'Args: {str(args)}')
		log.debug(f'Remainargv: {str(remainargv)}')

		return args, remainargv

	def __getattr__(self, name):
		global args
		global remainargv
		args, remainargv = self.get_args()
		return getattr(args, name)


args = ArgsWrapper()
remainargv = ''

K = 1024
M = 1024 * K
G = 1024 * M

class Config:
	_column_families = ['default', 'usertable']
	_config_templates  = {
		'08': {
			'db': {
				'bytes_per_sync'                        : str(8*M),
				'create_if_missing'                     : 'true',
				'create_missing_column_families'        : 'true',
				'db_write_buffer_size'                  : '0',
				'delayed_write_rate'                    : str(16*M),
				'enable_pipelined_write'                : 'true',
				'max_background_compactions'            : '4',
				'max_background_flushes'                : '7',
				'max_background_jobs'                   : '11',
				'max_open_files'                        : '-1',
				'new_table_reader_for_compaction_inputs': 'true',
				'table_cache_numshardbits'              : '4',
				},
			'cf': {
				'compaction_pri'                        : 'kMinOverlappingRatio',
				'compaction_style'                      : 'kCompactionStyleLevel',
				'compression'                           : 'kZSTD',
				'compression_per_level'                 : 'kZSTD:kZSTD:kZSTD:kZSTD:kZSTD:kZSTD:kZSTD',
				'compression_opts'                      : '{level=2}',
				'hard_pending_compaction_bytes_limit'   : str(256*G),
				'level0_file_num_compaction_trigger'    : '4',
				'level0_slowdown_writes_trigger'        : '20',
				'level_compaction_dynamic_level_bytes'  : 'true',
				'max_bytes_for_level_base'              : str(512*M),
				'max_bytes_for_level_multiplier'        : '10',
				'max_compaction_bytes'                  : str(3*G),
				'max_write_buffer_number'               : '6',
				'merge_operator'                        : 'PutOperator',
				'min_write_buffer_number_to_merge'      : '2',
				'num_levels'                            : '6',
				'target_file_size_base'                 : str(64*M),
				'ttl'                                   : '0',
				'write_buffer_size'                     : str(128*M),
				},
			'tb': {
				'block_cache'                           : str(2*G),
				'block_size'                            : str(8*K),
				'cache_index_and_filter_blocks'         : 'true',
				'cache_index_and_filter_blocks_with_high_priority': 'true',
				'filter_policy'                         : 'bloomfilter:10:false',
				'metadata_block_size'                   : str(4*K),
				'no_block_cache'                        : 'false',
				'optimize_filters_for_memory'           : 'true',
				'pin_l0_filter_and_index_blocks_in_cache': 'true',
				},
		},
	}

	_config = None
	_config_header = ''

	def create_templates(self):
		pass

	def __init__(self, template, argv):
		self.create_templates()

		if template not in self._config_templates.keys():
			raise Exception(f'template "{args.template}" not found')

		self._config_header = \
			f'# rocksdb_config_gen:\n' + \
			f'#   Template: {template}\n' + \
			f'#   Modifiers: {" ".join(argv)}\n\n'

		config = self._config_templates[template].copy()
		for i in argv:
			self.parse_arg(config, i)
		self._config = config

	def parse_arg(self, config, arg_str):
		r = re.findall(r'(db:|cf:|tb:)?([^=]+)=(.*)', arg_str)
		log.debug(f'r = {r}')
		if len(r) > 0:
			section, name, value = r[0][0].strip(), r[0][1].strip(), r[0][2].strip()
			if section == '':
				for k in config.keys():
					if name in config[k].keys():
						section = k
						break
				if section == '':
					raise Exception(f'config parameter "{name}" without section')
			else:
				section = section[0:-1]
			if value != '':
				config[section][name] = value
			elif name in config[section].keys():
				del config[section][name]
		else:
			raise Exception(f'invalid config parameter "{arg_str}"')

	def __str__(self):
		if self._config is None: return ''

		ret = self._config_header
		ret += '[Version]\n' +\
  		       '  rocksdb_version=6.15.5\n' +\
  		       '  options_file_version=1.1\n\n'

		ret += '[DBOptions]\n'
		for k, v in self._config['db'].items():
			ret += f'  {k}={v}\n'
		ret += '\n'

		for cf in self._column_families:
			ret += f'[CFOptions "{cf}"]\n'
			for k, v in self._config['cf'].items():
				ret += f'  {k}={v}\n'
			ret += '\n'
			ret += f'[TableOptions/BlockBasedTable "{cf}"]\n'
			for k, v in self._config['tb'].items():
				ret += f'  {k}={v}\n'
			ret += '\n'

		return ret

	def save(self, filename):
		if filename in ['', 'stdout']:
			print(self)
		else:
			with open(filename, 'wt') as f:
				f.write(str(self))

def main() -> int:
	try:
		config = Config(args.template, remainargv)
		config.save(args.output)

	except Exception as e:
		if log.level == logging.DEBUG:
			exc_type, exc_value, exc_traceback = sys.exc_info()
			sys.stderr.write('main exception:\n' +
			                 ''.join(traceback.format_exception(exc_type, exc_value, exc_traceback)) + '\n')
		else:
			sys.stderr.write(str(e) + '\n')
		return 1
	return 0


# =============================================================================
if __name__ == '__main__':
	exit(main())
