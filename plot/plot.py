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
import matplotlib.pyplot as plt
import sqlite3
import numpy

class Options:
	format = 'png'
	save = False

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

	_file_id = None
	_num_at = None

	def __init__(self, filename):
		self._filename = filename
		self._data = dict()
		self._dbbench = list()
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

	def graph_db(self):
		if len(self._dbbench) == 0:
			return
		fig, ax = plt.subplots()
		fig.set_figheight(5)
		fig.set_figwidth(8)
		ax.grid()

		for i in range(0, len(self._dbbench)):
			X = [i['time']/60.0 for i in self._data['db_bench[{}]'.format(i)]]
			Y = [i['ops_per_s'] for i in self._data['db_bench[{}]'.format(i)]]
			ax.plot(X, Y, '-', lw=1, label='db {}, real'.format(i))

			if self._dbbench[i].get("sine_d") is not None:
				sine_a = coalesce(self._dbbench[i]['sine_a'], 0)
				sine_b = coalesce(self._dbbench[i]['sine_b'], 0)
				sine_c = coalesce(self._dbbench[i]['sine_c'], 0)
				sine_d = coalesce(self._dbbench[i]['sine_d'], 0)
				Y = [ sine_a * math.sin(sine_b * x + sine_c) + sine_d for x in X]
				ax.plot(X, Y, '-', lw=1, label='db {}, expect'.format(i))

		ax.set(title="db_bench throughput", xlabel="time (min)", ylabel="tx/s")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if Options.save:
			save_name = '{}_graph_db.{}'.format(self._filename.replace('.out', ''), Options.format)
			fig.savefig(save_name)
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
		ax.grid()

		for i in range(0, num_ycsb):
			X = [i['time']/60.0 for i in self._data['ycsb[{}]'.format(i)]]
			Y = [i['ops_per_s'] for i in self._data['ycsb[{}]'.format(i)]]
			ax.plot(X, Y, '-', lw=1, label='db {}'.format(i))

		ax.set(title="YCSB throughput", xlabel="time (min)", ylabel="tx/s")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if Options.save:
			save_name = '{}_graph_ycsb.{}'.format(self._filename.replace('.out', ''), Options.format)
			fig.savefig(save_name)
		plt.show()

	def graph_io(self):
		if self._data.get('iostat') is None:
			return
		fig, axs = plt.subplots(3, 1)
		fig.set_figheight(5)
		fig.set_figwidth(8)
		axs[0].grid()
		axs[1].grid()
		axs[2].grid()

		X = [i['time']/60.0 for i in self._data['iostat']]
		Y = [i['rMB/s']     for i in self._data['iostat']]
		axs[0].plot(X, Y, '-', lw=1, label='read')
		Y = [i['wMB/s']     for i in self._data['iostat']]
		axs[0].plot(X, Y, '-', lw=1, label='write')
		axs[0].set(title="iostat", ylabel="MB/s")

		Y = [i['r/s']     for i in self._data['iostat']]
		axs[1].plot(X, Y, '-', lw=1, label='read')
		Y = [i['w/s']     for i in self._data['iostat']]
		axs[1].plot(X, Y, '-', lw=1, label='write')
		axs[1].set(ylabel="IO/s")

		Y = [i['%util']     for i in self._data['iostat']]
		axs[2].plot(X, Y, '-', lw=1, label='%util')
		axs[2].set(xlabel="time (min)", ylabel="percent")
		axs[2].set_ylim([-5, 105])

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

		axs[0].legend(loc='upper right', ncol=2, frameon=True)
		axs[1].legend(loc='upper right', ncol=2, frameon=True)
		axs[2].legend(loc='upper right', ncol=1, frameon=True)

		fig.tight_layout()

		if Options.save:
			save_name = '{}_graph_io.{}'.format(self._filename.replace('.out', ''), Options.format)
			fig.savefig(save_name)
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

		axs[0].set_ylim([-5, None])
		axs[1].set_ylim([-5, 105])
		axs[0].set(title="cpu", ylabel="all CPUs (%)")
		axs[1].set(xlabel="time (min)", ylabel="per CPU (%)")

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)

		axs[0].legend(loc='upper right', ncol=2, frameon=True)

		if Options.save:
			save_name = '{}_graph_cpu.{}'.format(self._filename.replace('.out', ''), Options.format)
			fig.savefig(save_name)
		plt.show()

	def graph_at3(self):
		if self._num_at == 0 or self._num_at is None:
			return

		fig, axs = plt.subplots(self._num_at, 1)
		a = 0.97
		fig.set_figheight(a * self._num_at + (6 - 6*a))
		fig.set_figwidth(8)

		for i in range(0,self._num_at):
			ax = axs[i] if self._num_at > 1 else axs
			ax.grid()
			cur_at = self._data['access_time3[{}]'.format(i)]
			X = [j['time']/60.0 for j in cur_at]
			Y = [j['total_MiB/s'] for j in cur_at]
			ax.plot(X, Y, '-', lw=1, label='total')
			Y = [j['read_MiB/s'] for j in cur_at]
			ax.plot(X, Y, '-.', lw=1, label='read')
			Y = [j['write_MiB/s'] for j in cur_at]
			ax.plot(X, Y, '-.', lw=1, label='write')

			ax_set = dict()
			ax_set['ylabel'] ="MiB/s"

			if i == 0:
				ax_set['title'] = "access_time3"
			if i == self._num_at -1:
				ax_set['xlabel'] = "time (min)"
				ax.legend(bbox_to_anchor=(0., -1.2, 1., .102), loc='lower left',
					ncol=3, mode="expand", borderaxespad=0.)
			if i>=0 and i < self._num_at -1:
				ax.xaxis.set_ticklabels([])

			ax.set(**ax_set)
			#ax.set_yscale('log')

			#chartBox = ax.get_position()
			#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.75, chartBox.height])
			#ax.legend(loc='upper center', bbox_to_anchor=(1.25, 1.0), ncol=2, frameon=True)

		if Options.save:
			save_name = '{}_graph_at3.{}'.format(self._filename.replace('.out', ''), Options.format)
			fig.savefig(save_name)
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
				sql1 = '''SELECT write_ratio, AVG(mbps) * {}
					FROM data
					WHERE file_id = {} AND block_size = {} AND random_ratio = {}
					GROUP BY write_ratio ORDER BY write_ratio'''
				q1 = DB.query(sql1.format(self._num_at, self._file_id, bs, rr))
				sql2 = '''SELECT write_ratio, AVG(mbps)
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
				xlabel='writes/reads', ylabel='MiB/s')

			chartBox = ax.get_position()
			ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
			ax.legend(loc='upper center', bbox_to_anchor=(1.25, 0.9), ncol=1, frameon=True)
			#ax.legend(loc='best', ncol=1, frameon=True)

			if Options.save:
				save_name = '{}-at3_bs{}.{}'.format(self._filename.replace('.out', ''), bs, Options.format)
				fig.savefig(save_name)
			plt.show()

	def graph_all(self):
		self.graph_db()
		self.graph_ycsb()
		self.graph_io()
		self.graph_cpu()
		self.graph_at3()

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


##############################################################################
Options.save = True

def getFiles(dirname):
	files = []
	os.chdir(dirname)
	for fn in os.listdir():
		if re.search(r'\.out$', fn) is not None:
			files.append(fn)
	return files

files = getFiles('exp_at3.direct_io')

for i in files:
	f = File('{}'.format(i))
	Options.format = 'png'
	f.graph_all()
	#f.graph_at3_write_ratio()
	Options.format = 'pdf'
	f.graph_all()
	#f.graph_at3_write_ratio()
	del f
