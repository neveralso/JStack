#!/usr/bin/env python3
# encoding: utf-8

import configparser
import os
import signal
import sys

import judge.core


def print_help():
    print('usage: judged start|status|stop')

if __name__ == '__main__':

    judge_config = configparser.ConfigParser()
    judge_config.read('conf/judged.conf')

    pid_file_path = os.path.join(os.getcwd(), judge_config['run']['pid_file'])
    judge_logger = judge.core.init_logger(judge_config)

    if len(sys.argv) < 2:
        print_help()
        exit(0)
    elif sys.argv[1] == 'start':
        judge.core.start_daemon(judge_config, judge_logger)
    elif sys.argv[1] == 'status':
        pass
    elif sys.argv[1] == 'stop':
        try:
            pid_file = open(pid_file_path, 'r')
            pid = int(pid_file.read())
            os.kill(pid, signal.SIGKILL)
            print('Judge daemon(pid=%d) has bing killed' % pid)
            judge_logger.info('Judge daemon(pid=%d) has bing killed' % pid)
            pid_file.close()
            os.remove(pid_file_path)
        except FileNotFoundError as e:
            print('Process is not running')
    else:
        print_help()
    exit(0)
