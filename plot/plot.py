#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sat May 16 08:54:54 2020

@author: Adriano Lange <alange0001@gmail.com>
"""

import math
import collections
import re
import json
import matplotlib.pyplot as plt
import numpy

class Options:
	format = 'png'

class File:
	_filename = None
	_data = dict()
	_dbbench = collections.OrderedDict()

	def __init__(self, filename):
		self._filename = filename
		self.getDBBenchParams()
		self.loadData()

	def loadData(self):
		with open(self._filename) as file:
			for line in file.readlines():
				parsed_line = re.findall(r'Task ([^,]+), STATS: (.+)', line)
				if len(parsed_line) > 0:
					task = parsed_line[0][0]
					data = json.loads(parsed_line[0][1])
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
		initiated = False
		with open(self._filename) as file:
			for line in file.readlines():
				if not initiated:
					parsed_line = re.findall(r'Executing *db_bench. *Command:', line)
					if len(parsed_line) > 0:
						initiated = True
						continue
				if initiated:
					parsed_line = re.findall(r'^\[.*', line)
					if len(parsed_line) > 0:
						break
					for l2 in line.split("--"):
						parsed_line = re.findall(r'\s*([^=]+)="([^"]+)"', l2)
						if len(parsed_line) > 0:
							self._dbbench[parsed_line[0][0]] = tryConvert(parsed_line[0][1], int, float)
							continue
						parsed_line = re.findall(r'\s*([^=]+)=([^ ]+)', l2)
						if len(parsed_line) > 0:
							self._dbbench[parsed_line[0][0]] = tryConvert(parsed_line[0][1], int, float)
							continue

	def graph1(self, save=False):
		fig, ax = plt.subplots()
		fig.set_figheight(5)
		fig.set_figwidth(8)
		ax.grid()

		X = [i['time']      for i in self._data['dbbench']]
		Y = [i['ops_per_s'] for i in self._data['dbbench']]
		ax.plot(X, Y, '-', lw=1, label='ops_per_s')

		Y = [ self._dbbench['sine_a'] * math.sin(self._dbbench['sine_b'] * x) + self._dbbench['sine_d'] for x in X]
		ax.plot(X, Y, '-', lw=1, label='sine')

		#ax.set(title='fs%={FilesystemPercent}, threads={NumberOfFiles}'.format(
		#	**self.metadata
		#	), xlabel='writes/reads', ylabel='MiB/s')

		#chartBox = ax.get_position()
		#ax.set_position([chartBox.x0, chartBox.y0, chartBox.width*0.65, chartBox.height])
		#ax.legend(loc='upper center', bbox_to_anchor=(1.35, 0.9), title='threads', ncol=1, frameon=True)
		ax.legend(loc='best', ncol=1, frameon=True)

		if save:
			save_name = '{}.{}'.format(self._filename, Options.format)
			fig.savefig(save_name)
		plt.show()

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

f = File('data1/out6')
f.graph1()
