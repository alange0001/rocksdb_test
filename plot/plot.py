#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sat May 16 08:54:54 2020

@author: Adriano Lange <alange0001@gmail.com>
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

class File:
	_filename = None
	_stats_interval = None
	_data = None
	_dbbench = None

	_plotdata = None #get data from generated graphs

	_file_id = None
	_num_at = None

	def __init__(self, filename):
		self._filename = filename
		self._data = dict()
		self._dbbench = list()
		self._plotdata = collections.OrderedDict()
		self.getDBBenchParams()
		self.loadData()

	def loadData(self):
		with open(self._filename) as file:
			for line in file.readlines():
				if self._stats_interval is None:
					parsed_line = re.findall(r'Args\.stats_interval: *([0-9]+)', line)
					if len(parsed_line) > 0:
						self._stats_interval = tryConvert(parsed_line[0][0], int, float)
				if self._num_at is None:
					parsed_line = re.findall(r'Args\.num_at: *([0-9]+)', line)
					if len(parsed_line) > 0:
						self._num_at = tryConvert(parsed_line[0][0], int, float)
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
		if Options.savePlotData:
			self._plotdata[name] = data

	def graph_db(self):
		if len(self._dbbench) == 0:
			return
		fig, ax = plt.subplots()
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for i in range(0, len(self._dbbench)):
			X = [i['time']/60.0 for i in self._data['db_bench[{}]'.format(i)]]
			Y = [i['ops_per_s'] for i in self._data['db_bench[{}]'.format(i)]]
			ax.plot(X, Y, '-', lw=1, label='db {}, real'.format(i))

			self.savePlotData('db_X', X)
			self.savePlotData('db_Y', Y)

			if self._dbbench[i].get("sine_d") is not None:
				sine_a = coalesce(self._dbbench[i]['sine_a'], 0)
				sine_b = coalesce(self._dbbench[i]['sine_b'], 0)
				sine_c = coalesce(self._dbbench[i]['sine_c'], 0)
				sine_d = coalesce(self._dbbench[i]['sine_d'], 0)
				Y = [ sine_a * math.sin(sine_b * x + sine_c) + sine_d for x in X]
				ax.plot(X, Y, '-', lw=1, label='db {}, expect'.format(i))

		aux = (X[-1] - X[0]) * 0.01
		ax.set_xlim([X[0]-aux,X[-1]+aux])

		self.setXticks(ax)

		ax.set(title="db_bench throughput", xlabel="time (min)", ylabel="tx/s")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if Options.save:
			for f in Options.formats:
				save_name = '{}_graph_db.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_ycsb(self):
		num_ycsb = 0
		for i in range(0,1024):
			if self._data.get('ycsb[{}]'.format(i)) is None:
				break
			num_ycsb += 1
		if num_ycsb == 0: return

		fig, ax = plt.subplots()
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for i in range(0, num_ycsb):
			X = [i['time']/60.0 for i in self._data['ycsb[{}]'.format(i)]]
			Y = [i['ops_per_s'] for i in self._data['ycsb[{}]'.format(i)]]
			ax.plot(X, Y, '-', lw=1, label='db {}'.format(i))

		aux = (X[-1] - X[0]) * 0.01
		ax.set_xlim([X[0]-aux,X[-1]+aux])

		self.setXticks(ax)

		ax.set(title="YCSB throughput", xlabel="time (min)", ylabel="tx/s")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if Options.save:
			for f in Options.formats:
				save_name = '{}_graph_ycsb.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_io(self):
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
				ax.set(title="iostat", ylabel="MiB/s")
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

		if Options.save:
			for f in Options.formats:
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

		if Options.save:
			for f in Options.formats:
				save_name = '{}_graph_io_norm.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_cpu(self):
		if self._data['systemstats'][0].get('cpus.active') is None:
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

		if Options.save:
			for f in Options.formats:
				save_name = '{}_graph_cpu.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_at3(self):
		if self._num_at == 0 or self._num_at is None:
			return

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
			ax_set['ylabel'] ="MiB/s"

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

		if Options.save:
			for f in Options.formats:
				save_name = '{}_graph_at3.{}'.format(self._filename.replace('.out', ''), f)
				fig.savefig(save_name, bbox_inches="tight")
		plt.show()

	def graph_at3_script(self):
		if self._num_at == 0 or self._num_at is None:
			return

		fig, axs = plt.subplots(self._num_at, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)

		for i in range(0,self._num_at):
			ax = axs[i] if self._num_at > 1 else axs
			ax.grid()
			cur_at = self._data['access_time3[{}]'.format(i)]
			X = [j['time']/60.0 for j in cur_at]
			Y = [j['write_ratio']*100 if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-', lw=1.5, label='% write', color='orange')
			Y = [j['random_ratio']*100 if j['wait'] == 'false' else None for j in cur_at]
			ax.plot(X, Y, '-.', lw=1.5, label='% random', color='blue')

			ax_set = dict()
			ax_set['ylabel'] ="%"

			if i == 0:
				ax_set['title'] = "access_time3: access pattern"
			if i == self._num_at -1:
				ax_set['xlabel'] = "time (min)"
				ax.legend(bbox_to_anchor=(0., -.8, 1., .102), loc='lower left',
					ncol=2, mode="expand", borderaxespad=0.)
			if i>=0 and i < self._num_at -1:
				ax.xaxis.set_ticklabels([])

			aux = (X[-1] - X[0]) * 0.01
			ax.set_xlim([X[0]-aux,X[-1]+aux])
			ax.set_ylim([-5,108])

			self.setXticks(ax)

			ax.set(**ax_set)
			#ax.set_yscale('log')

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.75, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.25, 1.0), ncol=2, frameon=True)

		plt.subplots_adjust(hspace=0.1)

		if Options.save:
			for f in Options.formats:
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

			ax.set(title='jobs={}, bs={}'.format(self._num_at, bs),
				xlabel='(writes/reads)*100', ylabel='MiB/s')

			chartBox = ax.get_position()
			ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
			ax.legend(loc='upper center', bbox_to_anchor=(1.25, 0.9), ncol=1, frameon=True)
			#ax.legend(loc='best', ncol=1, frameon=True)

			if Options.save:
				for f in Options.formats:
					save_name = '{}-at3_bs{}.{}'.format(self._filename.replace('.out', ''), bs, f)
					fig.savefig(save_name, bbox_inches="tight")
			plt.show()

	def setXticks(self, ax):
		if Options.graphTickMajor is not None:
			ax.xaxis.set_major_locator(MultipleLocator(Options.graphTickMajor))
			ax.xaxis.set_minor_locator(AutoMinorLocator(Options.graphTickMinor))
			ax.grid(which='major', color='#CCCCCC', linestyle='--')
			ax.grid(which='minor', color='#CCCCCC', linestyle=':')

	def graph_all(self):
		## Generic Graphs:
		self.graph_db()
		self.graph_ycsb()
		self.graph_io()
		self.graph_cpu()
		self.graph_at3()
		self.graph_at3_script()

		## Special case graphs:
		#self.graph_io_norm()
		#self.graph_at3_write_ratio()

def coalesce(*values):
	for v in values:
		if v is not None:
			return v;
	return None

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

def getFiles(dirname):
	files = []
	os.chdir(dirname)
	for fn in os.listdir():
		if re.search(r'\.out$', fn) is not None:
			files.append(fn)
	return files

##############################################################################
class Options:
	formats = ['png', 'pdf']
	save = True
	savePlotData = False
	graphTickMajor = 5
	graphTickMinor = 5

filenames = getFiles('.')
#filenames = ['exp_db/ycsb_wb,at3_bs512_directio.out']
files = collections.OrderedDict()

for name in filenames:
	f = File(name)
	f.graph_all()
	files[name] = f
	del f
