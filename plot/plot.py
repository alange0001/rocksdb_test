#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sat May 16 08:54:54 2020

@author: Adriano Lange <alange0001@gmail.com>

Copyright (c) 2020-present, Adriano Lange.  All rights reserved.
This source code is licensed under both the GPLv2 (found in the
LICENSE.GPLv2 file in the root directory) and Apache 2.0 License
(found in the LICENSE.Apache file in the root directory).
"""

import os
import math
import collections
import re
import json
import sqlite3
import numpy
import matplotlib.pyplot as plt
from matplotlib.ticker import (AutoMinorLocator, MultipleLocator)
from mpl_toolkits.axes_grid1 import host_subplot
import pandas as pd

class Options:
	formats = ['png', 'pdf']
	print_params = False
	save = False
	savePlotData = False
	graphTickMajor = 5
	graphTickMinor = 5
	plot_db = True
	db_mean_interval = 2
	db_xlim = None
	db_ylim = None
	file_start_time = None
	plot_ycsb = True
	plot_io = True
	plot_cpu = True
	plot_at3 = True
	plot_at3_script = True
	plot_io_norm = False
	plot_at3_write_ratio = False
	plot_pressure = False
	plot_containers_io = True
	pressure_decreased = True
	use_at3_counters = True
	fio_folder = None
	def __init__(self, **kargs):
		if self.file_start_time is None:
			self.file_start_time = {}
		for k,v in kargs.items():
			if k == 'plot_nothing':
				if v:
					for i in dir(self):
						if 'plot_' in i:
							self.__setattr__(i, False)
			elif k in dir(self):
				self.__setattr__(k, v)
			else:
				raise Exception('Invalid option name: {}'.format(k))

class DBClass:
	conn = sqlite3.connect(':memory:')
	file_id = 0

	def __init__(self):
		cur = self.conn.cursor()
		cur.execute('''CREATE TABLE files (
			  file_id INT PRIMARY KEY, name TEXT,
			  number INT)''')
		cur.execute('''CREATE TABLE data (
			file_id INT, number INT, time INT,
			block_size INT, random_ratio DOUBLE, write_ratio DOUBLE,
			mbps DOUBLE, mbps_read DOUBLE, mbps_write DOUBLE, blocks_ps DOUBLE,
			PRIMARY KEY(file_id, number, time))''')
		self.conn.commit()

	def getFileId(self):
		ret = self.file_id
		self.file_id += 1
		return ret

	def getCursor(self):
		return self.conn.cursor()

	def query(self, sql, printsql=False):
		if printsql:
			print('SQL: ' + sql)
		return self.conn.cursor().execute(sql)

	def commit(self):
		self.conn.commit()

DB = DBClass()

class AllFiles:
	_options = None
	_dbmean = None
	_filename = None
	def __init__(self, filename):
		self._dbmean = []
		self._filename = filename
	def setOptions(self, options):
		self._options = options
	def add_dbmean_data(self, label, X, Y, W_ticks, W_labels):
		ret = {
			'label':label,
			'X':X,
			'Y':Y,
			'W_ticks':W_ticks,
			'W_labels':W_labels}
		self._dbmean.append(ret)
		return ret
	def set_dblim(self, X, Y):
		self._xlim = X
		self._ylim = Y
	def plot_dbmean(self):
		if len(self._dbmean) == 0:
			return

		fig = plt.gcf()
		#fig, ax = plt.subplots()
		ax = host_subplot(111, figure=fig)
		fig.set_figheight(3)
		fig.set_figwidth(8)

		for v in self._dbmean:
			X = v['X']
			Y = v['Y']
			ax.plot(X, Y, '-', lw=1, label=v['label'])

		if v.get('W_ticks') is not None:
			ax2 = ax.twin()
			ax2.set_xticks(v['W_ticks'])
			ax2.set_xticklabels(v['W_labels'], rotation=90)
			ax2.axis["right"].major_ticklabels.set_visible(False)
			ax2.axis["top"].major_ticklabels.set_visible(True)

		if self._options.db_xlim is not None:
			ax.set_xlim( self._options.db_xlim )
		if self._options.db_ylim is not None:
			ax.set_ylim( self._options.db_ylim )

		if self._options.graphTickMajor is not None:
			ax.xaxis.set_major_locator(MultipleLocator(self._options.graphTickMajor))
			ax.xaxis.set_minor_locator(AutoMinorLocator(self._options.graphTickMinor))
			ax.grid(which='major', color='#CCCCCC', linestyle='--')
			ax.grid(which='minor', color='#CCCCCC', linestyle=':')

		ax.set(xlabel="time (min)", ylabel="tx/s")

		ax.legend(loc='best', ncol=1, frameon=True)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_db.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

class File:
	_filename = None
	_options = None
	_allfiles = None
	_params = None

	_stats_interval = None
	_data = None
	_dbbench = None

	_plotdata = None #get data from generated graphs

	_file_id = None
	_num_at = None
	_at_direct_io = None

	def __init__(self, filename, options, allfiles=None):
		self._filename = filename
		self._options = options
		self._allfiles = allfiles
		if allfiles:
			self._allfiles.setOptions(options)
		self._params = collections.OrderedDict()
		self._data = dict()
		self._dbbench = list()
		self._plotdata = collections.OrderedDict()
		self.getDBBenchParams()
		self.loadData()

	def loadData(self):
		with open(self._filename) as file:
			for line in file.readlines():
				parsed_line = re.findall(r'Args\.([^:]+): *(.+)', line)
				if len(parsed_line) > 0:
					self._params[parsed_line[0][0]] = tryConvert(parsed_line[0][1], int, float)

				parsed_line = re.findall(r'Task ([^,]+), STATS: (.+)', line)
				if len(parsed_line) > 0:
					task = parsed_line[0][0]
					try:
						data = json.loads(parsed_line[0][1])
					except:
						print("json exception (task {}): {}".format(task, parsed_line[0][1]))
					#print("Task {}, data: {}".format(task, data))
					if self._data.get(task) is None:
						self._data[task] = []
					data_dict = collections.OrderedDict()
					self._data[task].append(data_dict)
					for k, v in data.items():
						data_dict[k] = tryConvert(v, int, float, decimalSuffix)
			for e in self._data.keys(): # delete the 1st data of each task
				del self._data[e][0]

		self._num_at = self._params['num_at']
		self._stats_interval = self._params['stats_interval']
		if self._num_at > 0:
			self._at_direct_io = (self._params['at_params[0]'].find('--direct_io') >= 0)

	def getDBBenchParams(self):
		num_dbs = 0
		cur_db = -1
		with open(self._filename) as file:
			for line in file.readlines():
				if num_dbs == 0:
					parsed_line = re.findall(r'Args\.num_dbs: *([0-9]+)', line) #number of DBs
					if len(parsed_line) > 0:
						num_dbs = int(parsed_line[0][0])
						for i in range(0, num_dbs):
							self._dbbench.append(collections.OrderedDict())
					continue
				parsed_line = re.findall(r'Executing *db_bench\[([0-9]+)\]. *Command:', line) # command of DB [i]
				if len(parsed_line) > 0:
					cur_db = int(parsed_line[0][0])
					continue
				parsed_line = re.findall(r'^\[.*', line) # end of the command
				if len(parsed_line) > 0:
					if cur_db == num_dbs -1: break
					else: continue
				for l2 in line.split("--"): # parameters
					parsed_line = re.findall(r'\s*([^=]+)="([^"]+)"', l2)
					if len(parsed_line) > 0:
						self._dbbench[cur_db][parsed_line[0][0]] = tryConvert(parsed_line[0][1], int, float)
						continue
					parsed_line = re.findall(r'\s*([^=]+)=([^ ]+)', l2)
					if len(parsed_line) > 0:
						self._dbbench[cur_db][parsed_line[0][0]] = tryConvert(parsed_line[0][1], int, float)
						continue

	def printParams(self):
		print('Params:')
		for k, v in self._params.items():
			if k.find('at_script') >= 0 : continue
			print('{:<20s}: {}'.format(k, v))
		print()

	def load_at3(self):
		file_id = DB.getFileId()
		self._file_id = file_id
		num_at = self._num_at

		DB.query("insert into files values ({}, '{}', {})".format(file_id, self._filename, num_at))
		for i in range(0, num_at):
			for j in self._data['access_time3[{}]'.format(i)]:
				values = collections.OrderedDict()
				values['file_id']      = file_id
				values['number']       = i
				values['time']         = j['time']
				values['block_size']   = j['block_size']
				values['random_ratio'] = j['random_ratio']
				values['write_ratio']  = j['write_ratio']
				values['mbps']         = j['total_MiB/s']
				values['mbps_read']    = j['read_MiB/s']
				values['mbps_write']   = j['write_MiB/s']
				values['blocks_ps']    = j['blocks/s'] if j.get('blocks/s') is not None else 'NULL'
				query = "insert into data (" + ', '.join(values.keys()) + ") values ({" + '}, {'.join(values.keys()) + "})"
				DB.query(query.format(**values))
		DB.commit()

	def savePlotData(self, name, data):
		if self._options.savePlotData:
			self._plotdata[name] = data

	def countDBs(self):
		num_dbbench, num_ycsb = 0, 0
		for i in range(0,1024):
			if self._data.get('db_bench[{}]'.format(i)) is None:
				break
			num_dbbench += 1
		for i in range(0,1024):
			if self._data.get('ycsb[{}]'.format(i)) is None:
				break
			num_ycsb += 1
		return (num_dbbench, num_ycsb)

	_count_at3 = None
	def countAT3(self):
		if self._count_at3 is None:
			num = 0
			for i in range(0,1024):
				if self._data.get('access_time3[{}]'.format(i)) is None:
					break
				num += 1
			self._count_at3 = num
		return self._count_at3

	_last_at3_time = 0
	_last_at3_indexes = None
	def lastAT3(self, time):
		count_at3 = self.countAT3()
		if time < self._last_at3_time or self._last_at3_indexes is None:
			self._last_at3_indexes = [ 0 for i in range (0, count_at3) ]
		ret = []
		for i in range(0, count_at3):
			at3 = self._data[f'access_time3[{i}]']
			last = at3[self._last_at3_indexes[i]]
			for j in range(self._last_at3_indexes[i], len(at3)):
				if at3[j]['time'] > time: break
				last = at3[j]
			ret.append(last)

		self._last_at3_time = time
		return ret

	def lastAT3str(self, time):
		at3list = self.lastAT3(time)

		ret = collections.OrderedDict()
		for i in at3list:
			if i['wait'] != 'true':
				#s = f'{i["random_ratio"]}r,{i["write_ratio"]}w'
				s = f'{i["random_ratio"]}r{i["write_ratio"]}w'
				if s in ret.keys():
					ret[s] += 1
				else:
					ret[s] = 1
		if len(ret) == 0:
			return 'w0'
		else:
			return '|'.join([ f'{v}x{k}' for k, v in ret.items() ])

	def getAT3ticks(self, Xmin, Xmax):
		ticks, labels = [], []
		if self.countAT3() > 0:
			last_w = ''
			last_count = 0
			for i in range(int(Xmin*60), int((Xmax*60)+1)):
				w = self.lastAT3str(i)
				if w != last_w:
					ticks.append(i/60.0)
					labels.append(f'$w_{{{last_count}}}$')
					last_count += 1
					last_w = w
		return (ticks, labels)

	def addAT3ticks(self, ax, Xmin, Xmax):
		if self.countAT3() > 0:
			X2_ticks, X2_labels = self.getAT3ticks(Xmin, Xmax)
			ax2 = ax.twin()
			ax2.set_xticks(X2_ticks)
			ax2.set_xticklabels(X2_labels, rotation=90)
			ax2.axis["right"].major_ticklabels.set_visible(False)
			ax2.axis["top"].major_ticklabels.set_visible(True)
			return ax2
		return None

	def getMean(self, X, Y, interval):
		pd1 = pd.DataFrame({
			'X': [(int(x/interval)*interval)+(interval/2) for x in X],
			'Y': Y,
			})
		pd2 = pd1.groupby(['X']).agg({'Y':'mean'}).sort_values('X')
		return list(pd2.index), list(pd2['Y'])

	def cutBegin(self, X, Y, start):
		retX, retY = [], []
		for i in range(len(X)):
			if X[i] >= start:
				retX.append(X[i] - start)
				retY.append(Y[i])
		return (retX, retY)

	def graph_db(self):
		num_dbbench, num_ycsb = self.countDBs()
		if num_dbbench == 0 and num_ycsb == 0:
			return

		fig = plt.gcf()
		#fig, ax = plt.subplots()
		ax = host_subplot(111, figure=fig)
		fig.set_figheight(3)
		fig.set_figwidth(8)

		Ymax = -1
		Xmin, Xmax = 10**10, -10**10
		allfiles_d = None
		for i in range(0, num_dbbench):
			X = [i['time']/60.0 for i in self._data[f'db_bench[{i}]']]
			Y = [i['ops_per_s'] for i in self._data[f'db_bench[{i}]']]

			if self._options.file_start_time is not None and self._options.file_start_time.get(self._filename) is not None:
				Xplot, Yplot = self.cutBegin(X, Y, self._options.file_start_time.get(self._filename))
			else:
				Xplot, Yplot = X, Y
			Xmin = min([Xmin, min(Xplot)])
			Xmax = max([Xmax, max(Xplot)])
			Ymax = max([Ymax, max(Yplot)])
			ax.plot(Xplot, Yplot, '-', lw=1, label=f'db_bench')

			if self._dbbench[i].get("sine_d") is not None:
				sine_a = coalesce(self._dbbench[i]['sine_a'], 0)
				sine_b = coalesce(self._dbbench[i]['sine_b'], 0)
				sine_c = coalesce(self._dbbench[i]['sine_c'], 0)
				sine_d = coalesce(self._dbbench[i]['sine_d'], 0)
				Y = [ sine_a * math.sin(sine_b * x + sine_c) + sine_d for x in X]
				if self._options.file_start_time is not None and self._options.file_start_time.get(self._filename) is not None:
					Xplot, Yplot = self.cutBegin(X, Y, self._options.file_start_time.get(self._filename))
				else:
					Xplot, Yplot = X, Y
				ax.plot(Xplot, Yplot, '-', lw=1, label=f'db_bench (expected)')

			if self._options.db_mean_interval is not None:
				X, Y = self.getMean(Xplot, Yplot, self._options.db_mean_interval)
				ax.plot(X, Y, '-', lw=1, label=f'db_bench mean')
				if i == 0 and self._allfiles is not None and self._params['num_at'] > 0:
					allfiles_d = self._allfiles.add_dbmean_data(f"bs{self._params['at_block_size[0]']}", X, Y, None, None)

		for i in range(0, num_ycsb):
			try:
				workload = self._params[f'ydb_workload[{i}]'].split('/')[-1]
				i_label = {'workloada': 'A', 'workloadb':'B'}[workload]
			except:
				i_label = i
			X = [i['time']/60.0 for i in self._data[f'ycsb[{i}]']]
			Y = [i['ops_per_s'] for i in self._data[f'ycsb[{i}]']]
			if self._options.file_start_time is not None and self._options.file_start_time.get(self._filename) is not None:
				Xplot, Yplot = self.cutBegin(X, Y, self._options.file_start_time.get(self._filename))
			else:
				Xplot, Yplot = X, Y
			Xmin = min([Xmin, min(Xplot)])
			Xmax = max([Xmax, max(Xplot)])
			Ymax = max([Ymax, max(Yplot)])
			ax.plot(Xplot, Yplot, '-', lw=1, label=f'ycsb {i_label}')

			if self._options.db_mean_interval is not None:
				X, Y = self.getMean(Xplot, Yplot, self._options.db_mean_interval)
				ax.plot(X, Y, '-', lw=1, label=f'ycsb {i_label} mean')
				if i == 0 and self._allfiles is not None and self._params['num_at'] > 0:
					allfiles_d = self._allfiles.add_dbmean_data(f"bs{self._params['at_block_size[0]']}", X, Y, None, None)

		if not(self._options.file_start_time is not None and self._options.file_start_time.get(self._filename) is not None):
			X2_ticks, X2_labels = self.getAT3ticks(Xmin, Xmax)
			ax2 = ax.twin()
			ax2.set_xticks(X2_ticks)
			ax2.set_xticklabels(X2_labels, rotation=90)
			ax2.axis["right"].major_ticklabels.set_visible(False)
			ax2.axis["top"].major_ticklabels.set_visible(True)
			if allfiles_d is not None:
				allfiles_d['W_ticks'] = X2_ticks
				allfiles_d['W_labels'] = X2_labels

		if self._options.db_xlim is not None:
			ax.set_xlim( self._options.db_xlim )
		if self._options.db_ylim is not None:
			ax.set_ylim( self._options.db_ylim )

		self.setXticks(ax)

		ax.set(xlabel="time (min)", ylabel="tx/s")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_db.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_io(self):
		self.graph_io_new()
		self.graph_io_old()

	def graph_io_new(self):
		if self._data.get('performancemonitor') is None:
			return

		X = [x['time']/60.0 for x in self._data['performancemonitor']]

		fig, axs = plt.subplots(3, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for ax_i in range(0,3):
			ax = axs[ax_i]
			if ax_i == 0:
				Yr = numpy.array([i['disk']['iostat']['rkB/s']/1024  for i in self._data['performancemonitor']])
				Yw = numpy.array([i['disk']['iostat']['wkB/s']/1024  for i in self._data['performancemonitor']])
				Yt = Yr + Yw
				ax.plot(X, Yr, '-', lw=1, label='read', color='green')
				ax.plot(X, Yw, '-', lw=1, label='write', color='orange')
				ax.plot(X, Yt, '-', lw=1, label='total', color='blue')
				ax.set(title="iostat", ylabel="MB/s")
				ax.legend(loc='upper right', ncol=3, frameon=True)
			elif ax_i == 1:
				Y = [i['disk']['iostat']['r/s']     for i in self._data['performancemonitor']]
				ax.plot(X, Y, '-', lw=1, label='read', color='green')
				Y = [i['disk']['iostat']['w/s']     for i in self._data['performancemonitor']]
				ax.plot(X, Y, '-', lw=1, label='write', color='orange')
				ax.set(ylabel="IO/s")
				ax.legend(loc='upper right', ncol=2, frameon=True)
			elif ax_i == 2:
				Y = [i['disk']['iostat']['util']     for i in self._data['performancemonitor']]
				ax.plot(X, Y, '-', lw=1, label='%util')
				ax.set(xlabel="time (min)", ylabel="percent")
				ax.set_ylim([-5, 105])
				ax.legend(loc='upper right', ncol=1, frameon=True)

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

			aux = (X[-1] - X[0]) * 0.01
			ax.set_xlim([X[0]-aux,X[-1]+aux])

			self.setXticks(ax)

		fig.tight_layout()

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_io.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_io_old(self):
		if self._data.get('iostat') is None:
			return
		fig, axs = plt.subplots(3, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for ax_i in range(0,3):
			ax = axs[ax_i]
			if ax_i == 0:
				X = [i['time']/60.0 for i in self._data['iostat']]
				Yr = numpy.array([i['rMB/s']     for i in self._data['iostat']])
				Yw = numpy.array([i['wMB/s']     for i in self._data['iostat']])
				Yt = Yr + Yw
				ax.plot(X, Yr, '-', lw=1, label='read', color='green')
				ax.plot(X, Yw, '-', lw=1, label='write', color='orange')
				ax.plot(X, Yt, '-', lw=1, label='total', color='blue')
				ax.set(title="iostat", ylabel="MB/s")
				ax.legend(loc='upper right', ncol=3, frameon=True)
			elif ax_i == 1:
				Y = [i['r/s']     for i in self._data['iostat']]
				ax.plot(X, Y, '-', lw=1, label='read', color='green')
				Y = [i['w/s']     for i in self._data['iostat']]
				ax.plot(X, Y, '-', lw=1, label='write', color='orange')
				ax.set(ylabel="IO/s")
				ax.legend(loc='upper right', ncol=2, frameon=True)
			elif ax_i == 2:
				Y = [i['%util']     for i in self._data['iostat']]
				ax.plot(X, Y, '-', lw=1, label='%util')
				ax.set(xlabel="time (min)", ylabel="percent")
				ax.set_ylim([-5, 105])
				ax.legend(loc='upper right', ncol=1, frameon=True)

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

			aux = (X[-1] - X[0]) * 0.01
			ax.set_xlim([X[0]-aux,X[-1]+aux])

			self.setXticks(ax)

		fig.tight_layout()

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_io.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_io_norm(self):
		if self._data.get('iostat') is None:
			return
		if self._num_at == 0 or self._num_at is None:
			return

		fig, ax = plt.subplots()
		fig.set_figheight(5)
		fig.set_figwidth(8)

		ax.grid()
		ax.set(ylabel="normalized performance")

		X = [i['time']/60.0 for i in self._data['iostat']]
		self.savePlotData('io_norm_total_X', X)
		Yr = numpy.array([i['rMB/s']     for i in self._data['iostat']])
		Yw = numpy.array([i['wMB/s']     for i in self._data['iostat']])
		Yt = Yr + Yw
		self.savePlotData('io_norm_total_Y_raw', Yt)
		Yt = Yt / Yt[0]
		self.savePlotData('io_norm_total_Y', Yt)
		ax.plot(X, Yt, '-', lw=1, label='device', color='blue')

		cur_at = self._data['access_time3[0]']
		X = [j['time']/60.0 for j in cur_at]
		Y = numpy.array([j['total_MiB/s'] if j['wait'] == 'false' else None for j in cur_at])
		Yfirst = None
		for i in Y:
			if i is not None:
				Yfirst = i
				break
		if Yfirst is not None and Yfirst != 0:
			Y = Y/Yfirst
			ax.plot(X, Y, '-', lw=1, label='job0', color='green')

		self.savePlotData('io_norm_job0_X', X)
		self.savePlotData('io_norm_job0_Y', Y)

		aux = (X[-1] - X[0]) * 0.01
		ax.set_xlim([X[0]-aux,X[-1]+aux])

		self.setXticks(ax)

		ax.legend(loc='upper right', ncol=3, frameon=True)

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

		fig.tight_layout()

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_io_norm.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_cpu(self):
		self.graph_cpu_new()
		self.graph_cpu_old()

	def graph_cpu_new(self):
		if 'performancemonitor' not in self._data.keys(): return

		def sum_active(percents):
			s = 0.
			for k, v in percents.items():
				if k not in ('idle', 'iowait', 'steal'):
					s += v
			return s

		fig, axs = plt.subplots(2, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)
		axs[0].grid()
		axs[1].grid()

		X = [x['time']/60.0 for x in self._data['performancemonitor']]
		Y = [ sum_active(x['cpu']['percent_total']) for x in self._data['performancemonitor'] ]
		#print(Y)
		axs[0].plot(X, Y, '-', lw=1, label='usage (all)')

		Y = [i['cpu']['percent_total']['iowait'] for i in self._data['performancemonitor']]
		axs[0].plot(X, Y, '-', lw=1, label='iowait')

		for i in range(0, int(self._data['performancemonitor'][0]['cpu']['count'])):
			Y = [ sum_active(x['cpu']['percent'][i]) for x in self._data['performancemonitor'] ]
			axs[1].plot(X, Y, '-', lw=1, label='cpu{}'.format(i))

		aux = (X[-1] - X[0]) * 0.01
		for ax in axs:
			ax.set_xlim([X[0]-aux,X[-1]+aux])
			self.setXticks(ax)

		axs[0].set_ylim([-5, None])
		axs[1].set_ylim([-5, 105])
		axs[0].set(title="cpu", ylabel="all CPUs (%)")
		axs[1].set(xlabel="time (min)", ylabel="per CPU (%)")

		axs[0].legend(loc='upper right', ncol=2, frameon=True)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_cpu.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_cpu_old(self):
		if 'systemstats' not in self._data.keys() or self._data['systemstats'][0].get('cpus.active') is None:
			return

		fig, axs = plt.subplots(2, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)
		axs[0].grid()
		axs[1].grid()

		X = [i['time']/60.0   for i in self._data['systemstats']]
		Y = [i['cpus.active'] for i in self._data['systemstats']]
		axs[0].plot(X, Y, '-', lw=1, label='usage (all)')

		Y = [i['cpus.iowait'] for i in self._data['systemstats']]
		axs[0].plot(X, Y, '-', lw=1, label='iowait')

		for i in range(0,1024):
			if self._data['systemstats'][0].get('cpu[{}].active'.format(i)) is None:
				break
			Y = [j['cpu[{}].active'.format(i)] for j in self._data['systemstats']]
			axs[1].plot(X, Y, '-', lw=1, label='cpu{}'.format(i))

		aux = (X[-1] - X[0]) * 0.01
		for ax in axs:
			ax.set_xlim([X[0]-aux,X[-1]+aux])
			self.setXticks(ax)

		axs[0].set_ylim([-5, None])
		axs[1].set_ylim([-5, 105])
		axs[0].set(title="cpu", ylabel="all CPUs (%)")
		axs[1].set(xlabel="time (min)", ylabel="per CPU (%)")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

		axs[0].legend(loc='upper right', ncol=2, frameon=True)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_cpu.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_at3(self):
		if self._num_at == 0 or self._num_at is None:
			return
		#print(f'graph_at3() filename: {self._filename}')

		fig, axs = plt.subplots(self._num_at, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for i in range(0,self._num_at):
			ax = axs[i] if self._num_at > 1 else axs
			ax.grid()
			cur_at = self._data['access_time3[{}]'.format(i)]
			X = [j['time']/60.0 for j in cur_at]
			Y = [j['total_MiB/s'] if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-', lw=1, label='total', color='blue')
			Y = [j['read_MiB/s'] if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-', lw=1, label='read', color='green')
			Y = [j['write_MiB/s'] if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-', lw=1, label='write', color='orange')

			ax_set = dict()
			ax_set['ylabel'] ="MB/s"

			if i == 0:
				ax_set['title'] = "access_time3: performance"
			if i == self._num_at -1:
				ax_set['xlabel'] = "time (min)"
				ax.legend(bbox_to_anchor=(0., -.8, 1., .102), loc='lower left',
					ncol=3, mode="expand", borderaxespad=0.)
			if i>=0 and i < self._num_at -1:
				ax.xaxis.set_ticklabels([])

			aux = (X[-1] - X[0]) * 0.01
			ax.set_xlim([X[0]-aux,X[-1]+aux])

			self.setXticks(ax)

			ax.set(**ax_set)
			#ax.set_yscale('log')

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.75, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.25, 1.0), ncol=2, frameon=True)

		plt.subplots_adjust(hspace=0.1)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_at3.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_at3_script(self):
		if self._num_at == 0 or self._num_at is None:
			return

		fig = plt.gcf()
		#fig, axs = plt.subplots(self._num_at, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for i in range(0,self._num_at):
			#ax = axs[i] if self._num_at > 1 else axs
			ax = host_subplot((100 * self._num_at) + 10+i+1, figure=fig)
			if i == 0:
				ax0 = ax

			ax.grid()
			cur_at = self._data['access_time3[{}]'.format(i)]
			X = [j['time']/60.0 for j in cur_at]
			Y = [j['write_ratio'] if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-', lw=1.5, label='write_ratio (wr)', color='orange')
			Y = [j['random_ratio'] if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-.', lw=1.5, label='random_ratio (rr)', color='blue')

			ax_set = dict()
			#ax_set['ylabel'] ="%"

			if i == 0:
				pass
				#ax_set['title'] = "access_time3: access pattern"
			if i == self._num_at -1:
				ax_set['xlabel'] = "time (min)"
				ax.legend(bbox_to_anchor=(0., -.8, 1., .102), loc='lower left',
					ncol=2, mode="expand", borderaxespad=0.)
			if i>=0 and i < self._num_at -1:
				ax.xaxis.set_ticklabels([])

			aux = (X[-1] - X[0]) * 0.01
			ax.set_xlim([X[0]-aux,X[-1]+aux])
			ax.set_ylim([-0.05,1.08])

			self.setXticks(ax)

			ax.set(**ax_set)
			#ax.set_yscale('log')

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.75, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.25, 1.0), ncol=2, frameon=True)

		self.addAT3ticks(ax0, X[0], X[-1])

		plt.subplots_adjust(hspace=0.1)

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_at3_script.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_at3_write_ratio(self):
		if self._num_at == 0 or self._num_at is None:
			return
		if self._file_id is None:
			self.load_at3()

		colors = plt.get_cmap('tab10').colors

		block_sizes = DB.query("select distinct block_size from data where file_id = {} order by block_size".format(self._file_id)).fetchall()
		random_ratios = DB.query("select distinct random_ratio from data where file_id = {} order by random_ratio".format(self._file_id)).fetchall()

		for i_bs in block_sizes:
			bs = i_bs[0]
			ci = 0
			fig, ax = plt.subplots()
			fig.set_figheight(5)
			fig.set_figwidth(8)
			ax.grid()
			for i_rr in random_ratios:
				rr = i_rr[0]
				sql1 = '''SELECT write_ratio*100, AVG(mbps) * {}
					FROM data
					WHERE file_id = {} AND block_size = {} AND random_ratio = {}
					GROUP BY write_ratio ORDER BY write_ratio'''
				q1 = DB.query(sql1.format(self._num_at, self._file_id, bs, rr))
				sql2 = '''SELECT write_ratio*100, AVG(mbps)
					FROM data
					WHERE file_id = {} AND block_size = {} AND random_ratio = {} AND number = 0
					GROUP BY write_ratio ORDER BY write_ratio'''
				q2 = DB.query(sql2.format(self._file_id, bs, rr))

				A = numpy.array(q1.fetchall()).T
				B = numpy.array(q2.fetchall()).T

				ax.plot(A[0], A[1], '-', color=colors[ci], lw=1, label='rand {}%, total'.format(int(rr*100)))
				ax.plot(B[0], B[1], '.-', color=colors[ci], lw=1, label='rand {}%, job0'.format(int(rr*100)))
				ci += 1

			ax.set(title='jobs={}, bs={}, {}'.format(self._num_at, bs, 'O_DIRECT+O_DSYNC' if self._at_direct_io else 'cache'),
				xlabel='(writes/reads)*100', ylabel='MB/s')

			chartBox = ax.get_position()
			ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
			ax.legend(loc='upper center', bbox_to_anchor=(1.25, 0.9), ncol=1, frameon=True)
			#ax.legend(loc='best', ncol=1, frameon=True)

			if self._options.save:
				for f in self._options.formats:
					save_name = '{}-at3_bs{}.{}'.format(self._filename.replace('.out', ''), bs, f)
					fig.savefig(save_name, bbox_inches="tight")
			plt.show()

	def getPressureData(self):
		num_dbbench, num_ycsb = self.countDBs()
		num_at3 = self.countAT3()
		if num_dbbench == 0 and num_ycsb == 0: return None
		if num_at3 == 0: return None

		target_attribute = 'ops_per_s'
		if num_ycsb > 0:
			target_db = self._data['ycsb[0]']
		else:
			target_db = self._data['db_bench[0]']

		target_data = collections.OrderedDict()
		last_w = ''
		last_w_counter = -1
		for i in target_db:
			target_data[i['time']] = collections.OrderedDict()
			target_data[i['time']]['db'] = i

			w = self.lastAT3str(i['time'])
			if w != last_w:
				last_w_counter += 1
				last_w = w
			#if self._options.use_at3_counters:
			target_data[i['time']]['at3'] = f'w{last_w_counter}'
			#else:
			#	target_data[i['time']]['at3'] = w
			target_data[i['time']]['at3_counter'] = last_w_counter

		timelist = list(target_data.keys()); timelist.sort()

		pd1 = pd.DataFrame({
			'time'      : timelist,
			'ops_per_s' : [ target_data[t]['db'][target_attribute] for t in timelist ],
			'w'         : [ target_data[t]['at3'] for t in timelist ],
			'w_counter' : [ target_data[t]['at3_counter'] for t in timelist ],
			})
		return pd1

	def graph_pressure(self):
		pd = self.getPressureData()
		if pd is None: return
		if self._options.use_at3_counters:
			pd2 = pd.groupby(['w', 'w_counter']).agg({'ops_per_s':'mean'}).sort_values('w_counter')
		else:
			pd2 = pd.groupby(['w', 'w_counter']).agg({'ops_per_s':'mean'}).sort_values('ops_per_s', ascending=False)

		fig = plt.figure()
		fig.set_figheight(2.8)
		fig.set_figwidth(9)

		ax = fig.add_axes([0,1,1,1])

		X_labels = [ x[0] for x in pd2.index ]
		X = range(len(X_labels))
		Y = [ i[0] for i in pd2.values ]
		ax.bar(X, Y, label='throughput')
		ax.set_xticks(X)
		ax.set_xticklabels(X_labels, rotation=90)

		ax.set(xlabel="$w_i$", ylabel="$\\rho(w_i)$")
		ax.legend(loc='upper center', ncol=1, frameon=False)

		ax2 = ax.twinx()
		ax2.set_yticks([ 0, 0.25, 0.5, 0.75, 1])
		ax2.grid()
		ax2.yaxis.set_visible(True)
		ax2.xaxis.set_visible(False)

		w0 = pd2.loc['w0', 0][0]
		Y2 = [ (w0 - i[0])/w0  for i in pd2.values ]
		ax2.plot(X, Y2, '-', label='normalized pressure', color='red')

		X3, Y3 = [], []
		min_p = Y2[0]
		for i in range(1, len(Y2)):
			if Y2[i] < min_p:
				X3.append(X[i])
				Y3.append(Y2[i])
			else:
				min_p = Y2[i]
		if self._options.pressure_decreased:
			ax2.plot(X3, Y3, '*', label='decreased', color='red')
		ax2.legend(loc='upper right', ncol=2, frameon=False)
		ax2.set(ylabel="normalized pressure")

		ax.set_ylim([0, 1.1*w0])
		ax2.set_ylim([min(0, min(Y2)), 1.1])


		######################################################
		ax = fig.add_axes([0,0.62,1,0.16])

		X = [ (w0 - i[0])/w0  for i in pd2.values ]
		Y = [ 0  for i in pd2.values ]
		ax.plot(X, Y, 'o', label='pressure')

		ax.set_xlim([min(0, min(X))-0.05, 1.05])
		ax.set_ylim([-0.02, 0.1])
		ax.yaxis.set_ticklabels([])

		for i in range(len(X)):
			ax.annotate(f'{X_labels[i]}', xy=(X[i], 0), xytext=(X[i]-0.006,0.035), rotation=90)

		#ax.grid()
		ax.xaxis.set_major_locator(MultipleLocator(0.1))
		ax.xaxis.set_minor_locator(AutoMinorLocator(4))
		ax.grid(which='major', color='#888888', linestyle='--')
		ax.grid(which='minor', color='#CCCCCC', linestyle=':')

		ax.set(xlabel="normalized pressure: $(\\rho(w_0)-\\rho(w_i)) / \\rho(w_0)$")

		global a
		a = ax

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}-pressure.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def getContainerNames(self):
		perfmon_data = self._data.get('performancemonitor')
		if perfmon_data is None: return

		container_names = []
		for d in perfmon_data:
			containers_data = d.get('containers')
			if containers_data is None: continue
			for k in containers_data.keys():
				if k not in container_names:
					container_names.append(k)
		return container_names

	def mapContainerNames(self):
		names = self.getContainerNames()
		stats_keys = self._data.keys()
		#print(f'stats_keys: {stats_keys}')

		ret = {'container_names':names, 'stats2container':{}, 'container2stats':{}}
		for n in names:
			r = re.findall(r'^([^_]+)_([0-9]+)$', n)
			if len(r) > 0:
				if r[0][0] == 'at3':
					name_stats = f'access_time3[{r[0][1]}]'
				else:
					name_stats = f'{r[0][0]}[{r[0][1]}]'
				if name_stats in stats_keys:
					ret['stats2container'][name_stats] = n
					ret['container2stats'][n] = name_stats
				else:
					print(f'WARNING: container name "{n}" without corresponding stats')
			else:
				print(f'WARNING: container name "{n}" without pattern')
		return ret

	def graph_containers_io(self):
		perfmon_data = self._data.get('performancemonitor')
		if perfmon_data is None: return

		containers_map = self.mapContainerNames()
		#print(f'containers_map: {containers_map}')
		if len(containers_map['container_names']) == 0: return
		containers_map['container_names'].sort()

		colors = plt.get_cmap('tab10').colors

		fig, axs = plt.subplots(len(containers_map['container_names'])+1, 1)
		fig.set_figheight(10)
		fig.set_figwidth(9)

		X = [x['time']/60.0 for x in self._data['performancemonitor']]
		sum_Y1r = numpy.array([0. for x in X])
		sum_Y1w, sum_Y2r, sum_Y2w = sum_Y1r.copy(), sum_Y1r.copy(), sum_Y1r.copy()

		for i in range(len(axs)):
			ax = axs[i]
			ax.grid()

			if i == (len(axs)-1):
				c_name = 'ALL'
				Y1r, Y1w = sum_Y1r, sum_Y1w
			else:
				c_name = containers_map['container_names'][i]
				Y1r = [scale(coalesceDict(x, 'containers',c_name,'blkio.service_bytes/s','Read'), 1024**2) for x in self._data['performancemonitor']]
				sum_Y1r += [coalesce(y,0.) for y in Y1r]
				Y1w = [scale(coalesceDict(x, 'containers',c_name,'blkio.service_bytes/s','Write'), 1024**2) for x in self._data['performancemonitor']]
				sum_Y1w += [coalesce(y,0.) for y in Y1w]
			ax.plot(X, Y1r, '-', lw=1, label=f'MiB read', color=colors[0])
			ax.plot(X, Y1w, '-.', lw=1, label=f'MiB write', color=colors[1])

			ax.set(ylabel=f"{c_name}\nMiB/s")

			ax2 = ax.twinx()

			if i == (len(axs)-1):
				Y2r, Y2w = sum_Y2r, sum_Y2w
			else:
				Y2r = [coalesceDict(x, 'containers',c_name,'blkio.serviced/s','Read') for x in self._data['performancemonitor']]
				sum_Y2r += [coalesce(y,0.) for y in Y2r]
				Y2w = [coalesceDict(x, 'containers',c_name,'blkio.serviced/s','Write') for x in self._data['performancemonitor']]
				sum_Y2w += [coalesce(y,0.) for y in Y2w]
			ax2.plot(X, Y2r, ':', lw=1, label=f'IO read', color=colors[2])
			ax2.plot(X, Y2w, ':', lw=1, label=f'IO write', color=colors[3])

			ax2.set(ylabel="IOPS")

			if i == (len(axs)-1):
				ax.legend(bbox_to_anchor=(0., -.8, .48, .102), loc='lower left',
					ncol=2, mode="expand", borderaxespad=0.)
				ax2.legend(bbox_to_anchor=(.52, -.8, .48, .102), loc='lower right',
					ncol=2, mode="expand", borderaxespad=0.)

		aux = (X[-1] - X[0]) * 0.01
		for ax in axs:
			ax.set_xlim([X[0]-aux,X[-1]+aux])
			self.setXticks(ax)

		axs[0].set(title="containers I/O")
		axs[-1].set(xlabel="time (min)")

		if self._options.save:
			for f in self._options.formats:
				save_name = '{}_graph_containers_io.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def setXticks(self, ax):
		if self._options.graphTickMajor is not None:
			ax.xaxis.set_major_locator(MultipleLocator(self._options.graphTickMajor))
			ax.xaxis.set_minor_locator(AutoMinorLocator(self._options.graphTickMinor))
			ax.grid(which='major', color='#CCCCCC', linestyle='--')
			ax.grid(which='minor', color='#CCCCCC', linestyle=':')

	def graph_all(self):
		if self._options.print_params:
			self.printParams()
		## Generic Graphs:
		if self._options.plot_db:         self.graph_db()
		if self._options.plot_io:         self.graph_io()
		if self._options.plot_cpu:        self.graph_cpu()
		if self._options.plot_at3:        self.graph_at3()
		if self._options.plot_at3_script: self.graph_at3_script()
		if self._options.plot_pressure:   self.graph_pressure()
		if self._options.plot_containers_io: self.graph_containers_io()

		## Special case graphs:
		# exp_at3_rww:
		if self._options.plot_io_norm: self.graph_io_norm()
		# exp_at3:
		if self._options.plot_at3_write_ratio: self.graph_at3_write_ratio()

def graph_at3_script(filename, num_at3, max_w):
	fig = plt.gcf()
	#fig, axs = plt.subplots(self._num_at, 1)
	fig.set_figheight(5)
	fig.set_figwidth(9)

	X = [j for j in range(0,max_w+1)]
	X_labels = [f'$w_{{{j}}}$' for j in range(0,max_w+1)]

	write_ratios = [0., 0.1, 0.2, 0.3, 0.5, 0.7, 1.]

	for i in range(0, num_at3):
		#ax = axs[i] if self._num_at > 1 else axs
		ax = host_subplot((100 * num_at3) + 10+i+1, figure=fig)
		#if i == 0:
		#	ax0 = ax

		ax.grid()

		cur_wr = 0
		X2, Y = [], []
		j2 = 0
		for j in range(0,max_w+1):
			if j <= i:
				pass
				#Y.append(None)
			else:
				X2.append(j)
				Y.append(write_ratios[cur_wr])
				j2 += 1
				if j2 >= num_at3:
					cur_wr += 1
					j2 = 0
		ax.plot(X2, Y, '-', lw=1.5, label='write_ratio ($wr$)', color='orange')
		#ax.bar(X2, Y, label='write_ratio (wr)', color='orange')

		Y = [0.5 if j>i else None for j in range(0,max_w+1)]
		ax.plot(X, Y, '-.', lw=1.5, label='random_ratio ($rr$)', color='blue')

		ax_set = dict()

		ax.set_xticks(X)
		if i == 0:
			pass
			#ax_set['title'] = "access_time3: access pattern"
		if i == num_at3 -1:
			ax_set['xlabel'] = "concurrent workloads ($W$)"
			ax.legend(bbox_to_anchor=(0., -.9, 1., .102), loc='lower left',
				ncol=2, mode="expand", borderaxespad=0.)
			ax.set_xticklabels(X_labels)
		if i>=0 and i < num_at3 -1:
			ax.xaxis.set_ticklabels([])

		ax.set_xlim([-0.5,  max_w+0.5])
		ax.set_ylim([-0.05, 1.08])

		ax.set(**ax_set)

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.75, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.25, 1.0), ncol=2, frameon=True)


	plt.subplots_adjust(hspace=0.1)

	save_name = filename
	fig.savefig(save_name, bbox_inches="tight")
	plt.show()

def coalesce(*values):
	for v in values:
		if v is not None:
			return v;
	return None

def coalesceDict(dict_v, *names):
	cur_d = dict_v
	for n in names:
		if cur_d.get(n) is not None:
			cur_d = cur_d.get(n)
		else:
			return None
	return cur_d

def scale(value, divisor):
	if value is not None:
		return value / divisor
	return None

def scaleList(values, divisor):
	return [scale(x, divisor) for x in values]

def tryConvert(value, *types):
	for t in types:
		try:
			ret = t(value)
			return ret
		except:
			pass
	return value

def decimalSuffix(value):
	r = re.findall(r' *([0-9.]+) *([TBMK]) *', value)
	if len(r) > 0:
		number = tryConvert(r[0][0], int, float)
		suffix = r[0][1]
		if   suffix == "K": number = number * 1000
		elif suffix == "M": number = number * (1000**2)
		elif suffix == "B": number = number * (1000**3)
		elif suffix == "T": number = number * (1000**4)
		return number
	else:
		raise Exception("invalid number")

def binarySuffix(value):
	r = re.findall(r' *([0-9.]+) *([PTGMKptgmk])i{0,1}[Bb]{0,1} *', value)
	if len(r) > 0:
		number = tryConvert(r[0][0], int, float)
		suffix = r[0][1]
		if   suffix.upper() == "K": number = number * 1024
		elif suffix.upper() == "M": number = number * (1024**2)
		elif suffix.upper() == "G": number = number * (1024**3)
		elif suffix.upper() == "T": number = number * (1024**4)
		elif suffix.upper() == "P": number = number * (1024**5)
		return number
	else:
		raise Exception("invalid number")

def getFiles(dirname):
	try:
		from natsort import natsorted
		sort_method = natsorted
	except:
		print('WARNING: natsort not installed, using sorted')
		sort_method = sorted
	files = []
	for fn in os.listdir(dirname):
		if re.search(r'\.out$', fn) is not None:
			files.append('{}/{}'.format(dirname, fn))
	return sort_method(files)

def plotFiles(filenames, options, allfiles=None):
	for name in filenames:
		print(
			'######################################################\n' +
			'Graphs from file "{}":'.format(name) +
			'\n')
		f = File(name, options, allfiles)
		f.graph_all()
		del f

class FioFiles:
	_options = None
	_files = None
	_data = None
	_pd = None

	def __init__(self, files, options):
		self._options = options
		self._files = []
		self._data = []

		for f in files:
			self.parseFile(f)

		self._pd = pd.DataFrame({
			"rw":           [i['jobs'][0]['job options']['rw']                         for i in self._data],
			"iodepth":      [tryConvert( i['jobs'][0]['job options']['iodepth'], int)  for i in self._data],
			"bs":           [binarySuffix( i['jobs'][0]['job options']['bs'])          for i in self._data],
			"error":        [tryConvert(i['jobs'][0]['error'], bool)                   for i in self._data],
			"bw_min":       [i['jobs'][0]['mixed']['bw_min']                           for i in self._data],
			"bw_max":       [i['jobs'][0]['mixed']['bw_max']                           for i in self._data],
			"bw_agg":       [i['jobs'][0]['mixed']['bw_agg']                           for i in self._data],
			"bw_mean":      [i['jobs'][0]['mixed']['bw_mean']                          for i in self._data],
			"bw_dev":       [i['jobs'][0]['mixed']['bw_dev']                           for i in self._data],
			"iops_min":     [i['jobs'][0]['mixed']['iops_min']                         for i in self._data],
			"iops_max":     [i['jobs'][0]['mixed']['iops_max']                         for i in self._data],
			"iops_mean":    [i['jobs'][0]['mixed']['iops_mean']                        for i in self._data],
			"iops_stddev":  [i['jobs'][0]['mixed']['iops_stddev']                      for i in self._data],
			"iops_samples": [i['jobs'][0]['mixed']['iops_samples']                     for i in self._data],
			})

	def parseFile(self, filename):
		try:
			with open(filename, "r") as f:
				j = json.load(f)
				self._files.append(filename)
				self._data.append(j)
		except Exception as e:
			print("failed to read file {}: {}".format(filename, str(e)))

	def sortPatterns(self, patterns):
		ret = []
		desired_order = ['read', 'randread', 'write', 'randwrite']
		for p in desired_order:
			if p in patterns: ret.append(p)
		for p in patterns:
			if p not in desired_order: ret.append(p)
		return ret

	def graph_bw(self):
		pattern_list = self.sortPatterns(self._pd['rw'].value_counts().index)
		for pattern in pattern_list:
			pattern_pd = self._pd[self._pd['rw'] == pattern]
			iodepth_list = list(pattern_pd['iodepth'].value_counts().index)
			iodepth_list.sort()
			X_values = list(pattern_pd['bs'].value_counts().index)
			X_values.sort()
			#print(X_values)

			fig, ax = plt.subplots()
			fig.set_figheight(4)
			fig.set_figwidth(9.8)

			X_labels = [ str(int(x/1024)) for x in X_values]
			#print(X_labels)

			Y_max = 0
			width=0.07
			s_width=0.0-((width * len(iodepth_list))/2)
			for iodepth in iodepth_list:
				X = [ x+s_width for x in range(0,len(X_values)) ]
				#print(X)
				Y = [ float(pattern_pd[(pattern_pd['iodepth']==iodepth)&(pattern_pd['bs']==x)]['bw_mean']) for x in X_values ]
				Y_dev = [ float(pattern_pd[(pattern_pd['iodepth']==iodepth)&(pattern_pd['bs']==x)]['bw_dev']) for x in X_values ]
				#print(Y)
				label = f'iodepth {iodepth}' if iodepth == 1 else f'{iodepth}'
				ax.bar(X, Y, yerr=Y_dev, label=label, width=width)
				Y_max = max([ Y_max, max(numpy.array(Y) + numpy.array(Y_dev)) ])
				s_width += width

			ax.set_xticks([ x for x in range(0, len(X_labels))])
			ax.set_xticklabels(X_labels)

			ax.set_ylim([0, Y_max * 1.2])

			ax.set(title="fio {}".format(pattern), xlabel="block size (KiB)", ylabel="KiB/s")
			ax.legend(loc='upper left', ncol=8, frameon=False)

			if self._options.save:
				folder = f'{self._options.fio_folder}/' if self._options.fio_folder is not None else ''
				for f in self._options.formats:
					save_name = f'{folder}fio_bw_{pattern}.{f}'
					fig.savefig(save_name, bbox_inches="tight")
			plt.show()

	def graph_iops(self):
		pattern_list = self.sortPatterns(self._pd['rw'].value_counts().index)
		for pattern in pattern_list:
			pattern_pd = self._pd[self._pd['rw'] == pattern]
			iodepth_list = list(pattern_pd['iodepth'].value_counts().index)
			iodepth_list.sort()
			X_values = list(pattern_pd['bs'].value_counts().index)
			X_values.sort()
			#print(X_values)

			fig, ax = plt.subplots()
			fig.set_figheight(4)
			fig.set_figwidth(9.8)

			X_labels = [ str(int(x/1024)) for x in X_values ]
			#print(X_labels)

			Y_max = 0
			width=0.07
			s_width=0.0-((width * len(iodepth_list))/2)
			for iodepth in iodepth_list:
				X = [ x+s_width for x in range(0,len(X_values)) ]
				#print(X)
				Y = [ float(pattern_pd[(pattern_pd['iodepth']==iodepth)&(pattern_pd['bs']==x)]['iops_mean']) for x in X_values ]
				Y_dev = [ float(pattern_pd[(pattern_pd['iodepth']==iodepth)&(pattern_pd['bs']==x)]['iops_stddev']) for x in X_values ]
				#print(Y)
				label = f'iodepth {iodepth}' if iodepth == 1 else f'{iodepth}'
				ax.bar(X, Y, yerr=Y_dev, label=label, width=width)
				Y_max = max([ Y_max, max(numpy.array(Y) + numpy.array(Y_dev)) ])
				s_width += width

			ax.set_xticks([ x for x in range(0, len(X_labels))])
			ax.set_xticklabels(X_labels)

			ax.set_ylim([0, Y_max * 1.2])

			ax.set(title="fio {}".format(pattern), xlabel="block size (KiB)", ylabel="IOPS")
			ax.legend(loc='upper left', ncol=8, frameon=False)

			if self._options.save:
				folder = f'{self._options.fio_folder}/' if self._options.fio_folder is not None else ''
				for f in self._options.formats:
					save_name = f'{folder}fio_iops_{pattern}.{f}'
					fig.savefig(save_name, bbox_inches="tight")
			plt.show()

##############################################################################
if __name__ == '__main__':
	pass
	#Options.save = True

	#graph_at3_script('at3_script25.pdf', 4, 25)
	#graph_at3_script('at3_script28.pdf', 4, 28)

	#plotFiles(getFiles('exp_db'), Options(plot_nothing=True, plot_pressure=True, db_mean_interval=2, pressure_decreased=False))

	#plotFiles(getFiles('exp_at3'), Options(plot_at3_write_ratio=True))
	#plotFiles(getFiles('exp_at3_rww'), Options(graphTickMajor=2, graphTickMinor=4, plot_io_norm=True))

	#options = Options(graphTickMajor=10, graphTickMinor=4)
	#plotFiles(["dbbench_mw2.out"], options)

	#Options.file_start_time['exp_db/ycsb_wa.out'] = 30
	#Options.file_start_time['exp_db/ycsb_wb.out'] = 30
	#Options.file_start_time['exp_db/dbbench_wwr.out'] = 30
	#Options.db_xlim = [-0.01,     60.01]
	#Options.db_ylim = [ 0   , 125000   ]
	#plotFiles(getFiles('exp_db'), Options(plot_nothing=True, plot_pressure=True, plot_db=True, db_mean_interval=2))
	##f = File('exp_db2/ycsb_wb.out', Options(plot_nothing=True, plot_db=True, db_mean_interval=2)); f.graph_all()
	#a = AllFiles()
	#f = File('exp_db/ycsb_wb,at3_bs64_directio.out', Options(plot_nothing=True, plot_db=True, db_mean_interval=2), a); f.graph_all()
	#f = File('exp_db/ycsb_wb,at3_bs128_directio.out', Options(plot_nothing=True, plot_db=True, db_mean_interval=2), a); f.graph_all()
	#a.plot_dbmean()

	#plotFiles(getFiles('exp_dbbench/rrwr'), Options(plot_nothing=True, plot_db=True, db_mean_interval=5))

	#f = File('../ycsb_workloadb,at3_bs512_directio.out', Options(plot_nothing=True, plot_containers_io=True, plot_io=True, plot_db=False, db_mean_interval=2)); f.graph_all()
	#f = File('exp_db/dbbench_wwr,at3_bs512_directio.out', Options(use_at3_counters=True))
	#f = File('dbbench_wwr.out', Options(plot_pressure=True, db_mean_interval=2)); f.graph_all()
	#f = File('exp_db/ycsb_wa,at3_bs32_directio.out', Options(plot_nothing=True, plot_pressure=True, db_mean_interval=2, pressure_decreased=False)); f.graph_all()
	#f = File('exp_db/ycsb_wb,at3_bs32_directio.out', Options(plot_pressure=True, db_mean_interval=2)); f.graph_all()
	#f = File('ycsb_wb,at3_bs32_directio.out', Options(plot_pressure=True, db_mean_interval=2)); f.graph_all()
	#f = File('ycsb_workloadb_threads5.out', Options(plot_pressure=True, db_mean_interval=2)); f.graph_all()
	#f = File('ycsb_workloada_threads5.out', Options(plot_pressure=True, db_mean_interval=2)); f.graph_all()

	#f = File('exp_db5min/ycsb_workloadb.out', Options(plot_pressure=True, graphTickMajor=10, graphTickMinor=4, plot_db_mean_interval=5))
	#f = File('ycsb_workloadb_threads5.out', Options())
	#f = File('ycsb_workloadb_threads8.out', Options())
	#p = f.getPressureData()
	#f.graph_pressure()
	#f.graph_at3_script()
	#f.graph_db()
	#f.graph_all()

	#fiofiles = FioFiles(getFiles('exp_fio'), Options(fio_folder='exp_fio'))
	#fiofiles.graph_bw()
	#fiofiles.graph_iops()
