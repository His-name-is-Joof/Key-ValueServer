#!/usr/bin/env python2

import logging
import signal
import socket
import subprocess
import sys
import threading
import time
import unittest

logger = logging.getLogger()

PROG_PATH = './server'
LIST_ADDR = '127.0.0.1'
LIST_PORT = '2020'

class EpochAPI(object):
    def __init__(self, addr, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((addr, int(port)))
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, True)

    def __del__(self):
        self.sock.close()

    def _send(self, msg):
        sent = self.sock.send(msg)
        assert len(msg) == sent

    def send_set(self, key, val):
        self._send('set %s %s\n' % (key, val))

    def send_get(self, key):
        self._send('get %s\n' % key)

    def send_quit(self):
        self._send('quit\n')

    def recv_msg(self):
        buf = ''
        while not buf.endswith('\n'):
            bytes_read = self.sock.recv(1)
            if len(bytes_read) == 0:
                raise IOError('short read, only received %s' % buf)
            buf += bytes_read
        return buf.strip()

    def assert_recv_msg(self, expected_key, expected_val):
        msg = self.recv_msg()
        if msg != ('%s=%s' % (expected_key, expected_val)):
            print 'recv: %s' % msg
            print 'expt: %s=%s' % (expected_key, expected_val)
        assert msg == ('%s=%s' % (expected_key, expected_val))


    def assert_set(self, key, val):
        self.send_set(key, val)
        self.assert_recv_msg(key, val)

    def assert_get(self, key, val):
        self.send_get(key)
        self.assert_recv_msg(key, val)

    def assert_quit(self):
        self.send_quit()
        bytes_read = self.sock.recv(1)
        assert len(bytes_read) == 0

    def assert_multi_response(self, tuples):
        for t in tuples:
            self.assert_recv_msg(t[0], t[1])

    def assert_multi_set(self, tuples):
        for t in tuples:
            self.send_set(t[0], t[1])
        self.assert_multi_response(tuples)

    def assert_multi_get(self, tuples):
        for t in tuples:
            self.send_get(t[0], t[1])
        self.assert_multi_response(tuples)

class TestServer(unittest.TestCase):
    def setUp(self):
        self.launch()
        self.api0 = EpochAPI(LIST_ADDR, LIST_PORT)
        self.api1 = EpochAPI(LIST_ADDR, LIST_PORT)

    def tearDown(self):
        if self.p.poll() == None:
            logger.debug("killing server")
            self.p.terminate()
            count = 0
            while self.p.poll() == None and count < 10:
                time.sleep(0.1)
                count = count + 1
            if self.p.poll() == None:
                self.p.kill()
                while self.p.poll() == None:
                    time.sleep(0.1)

        self.prog_out.seek(0)
        logger.debug("server logs:\n%s", self.prog_out.read())

        del self.p
        del self.api0
        del self.api1
        del self.prog_out

    def launch(self):
        logger.debug("launching server")
        self.prog_out = open('server.log', 'a+')
        self.prog_out.truncate()
        self.p = subprocess.Popen([PROG_PATH, LIST_ADDR, LIST_PORT], stdout=self.prog_out, stderr=self.prog_out)
        while self.p.poll() == None:
            with open('server.log', 'r') as log:
                if 'listening on %s:%s' % (LIST_ADDR, LIST_PORT) in log.read().splitlines():
                    break
                time.sleep(0.1)
                if self.p.poll() != None:
                    raise Exception('server failed to start')
        logger.debug('server started')

    def assert_disconnect(self, api):
        api.sock.close()
        time.sleep(1)
        assert self.p.poll() == None
        
    def test_empty_quit(self):
        self.api1.assert_quit()
        self.api0.assert_quit()

    def test_empty_disconnect(self):
        self.assert_disconnect(self.api1)
        self.assert_disconnect(self.api0)

    def test_disconnect_after_set(self):
        self.api0.assert_set('a'*100, 'b'*100)
        self.assert_disconnect(self.api0)

    def test_disconnect_after_get(self):
        self.api0.assert_set('a'*500, 'b'*500)
        self.api0.assert_get('a'*500, 'b'*500)
        self.assert_disconnect(self.api0)

    def test_example_session(self):
        self.api0.assert_set('foo', 'bar')
        self.api0.assert_get('foo', 'bar')
        self.api0.assert_get('bar', 'null')
        self.api0.assert_set('1a2b3c', 'foo')
        self.api0.assert_set('10', 'ten')
        self.api0.assert_get('foo', 'bar')
        self.api0.assert_set('foo', 'baz')
        self.api0.assert_get('ten', 'null')
        self.api0.assert_get('foo', 'baz')
        self.api0.assert_get('qux', 'null')
        self.api0.assert_quit()

    def test_pipeline(self):
        reqs = [
            ('get', 'k0'),
            ('set', 'k1', 'v1'),
            ('get', 'k1'),
            ('set', 'k0', 'v0'),
            ('set', 'k1', 'val1'),
            ('get', 'k0'),
            ('get', 'k1')
        ]
        rsps = [
            ('k0', 'null'),
            ('k1', 'v1'),
            ('k1', 'v1'),
            ('k0', 'v0'),
            ('k1', 'val1'),
            ('k0', 'v0'),
            ('k1', 'val1')
        ]
        msgs = map(lambda t: ' '.join(t), reqs)
        self.api0._send('\n'.join(msgs) + '\n')
        self.api0.assert_multi_response(rsps)
        self.api0.assert_set('k2', 'v2')
        self.api0.assert_set('k2', 'val2')
        self.api0.assert_get('k2', 'val2')

    def test_threaded_pipeline(self):
        v = lambda i: ('val%s' % i)*(i%100+1)

        def write_target(api, count):
            reqs = '\n'.join(['set k %s' % v(i) for i in range(1, count)])
            api._send(reqs)
            api._send('\nset k pipeline\n')

        def read_target(api, count):
            for i in range(1, count):
                api.assert_recv_msg('k', v(i))
            api.assert_recv_msg('k', 'pipeline')

        apis = []
        threads = []
        thread_pairs=2
        request_count = 5*1000
        for i in range(thread_pairs):
            api = EpochAPI(LIST_ADDR, LIST_PORT)
            threads.append(threading.Thread(target=read_target, args=(api, request_count)))
            threads.append(threading.Thread(target=write_target, args=(api, request_count)))
            apis.append(api)
        map(lambda t: t.start(), threads)
        map(lambda t: t.join(), threads)
        self.api0.assert_get('k', 'pipeline')
        self.api1.assert_get('k', 'pipeline')

    def test_tarpit(self):
        self.api0._send('s')
        time.sleep(0.25)
        self.api0._send('e')
        time.sleep(0.25)
        self.api0._send('t')
        time.sleep(0.25)
        self.api0._send(' ')
        time.sleep(0.25)
        self.api0._send('tar')
        time.sleep(0.5)
        self.api0._send(' ')
        time.sleep(0.25)
        self.api0._send('pit')
        time.sleep(0.25)
        self.api0._send('\n')

    def test_multi_connection_partial_tarpit(self):
        self.api0.assert_get('tar', 'null')
        self.api1.assert_get('tar', 'null')
        self.api0._send('s')
        self.api1._send('g')
        time.sleep(0.25)
        self.api0._send('e')
        self.api1._send('e')
        time.sleep(0.25)
        self.api0._send('t')
        self.api1._send('t')
        time.sleep(0.25)
        self.api0._send(' ')
        self.api1._send(' ')
        time.sleep(0.25)
        self.api0._send('tar')
        self.api1._send('tar')
        time.sleep(0.25)
        self.api0._send(' ')
        time.sleep(0.25)
        self.api0._send('pit')
        time.sleep(0.25)
        self.api0._send('\n')
        self.api0.assert_recv_msg('tar', 'pit')
        self.api1._send('\n')
        self.api1.assert_recv_msg('tar', 'pit')

    def test_multi_connection_tarpit(self):
        apis = []
        for i in range(10):
            api = EpochAPI(LIST_ADDR, LIST_PORT)
            api.assert_get('bar', 'null')
            api._send('set ')
            time.sleep(0.25)
            api._send('foo ')
            time.sleep(0.25)
            #no newline
            api._send('barbaz')
            apis.append(api)

        for a in [self.api0, self.api1]:
            a.assert_get('foo', 'null')
            a.assert_set('foo', 'baz')
            a.assert_get('foo', 'baz')
            a.assert_set('foo', 'quz')
            a.assert_get('foo', 'quz')
            a.assert_set('foo', 'null')
            a.assert_get('foo', 'null')

        for a in apis:
            a._send('qux\n')
            a.assert_recv_msg('foo', 'barbazqux')
            a.assert_get('foo', 'barbazqux')
            self.api0.assert_get('foo', 'barbazqux')
            self.api0.assert_set('foo', 'foo')
            self.api1.assert_get('foo', 'foo')
            self.api1.assert_set('foo', 'barbazqux')
            self.api0.assert_get('foo', 'barbazqux')

        for a in apis:
            a.assert_quit()

    def test_cross_connection_xfer(self):
        self.api0.assert_get('foo', 'null')
        self.api1.assert_get('foo', 'null')
        self.api0.assert_set('foo', 'bar')
        self.api1.assert_get('foo', 'bar')
        self.api1.assert_set('foo', 'baz')
        self.api0.assert_get('foo', 'baz')

    def test_reconnect(self):
        self.api0.assert_get('foo', 'null')
        self.api0.assert_set('foo', 'bar')
        self.api0.assert_quit()
        self.api0 = EpochAPI(LIST_ADDR, LIST_PORT)
        self.api0.assert_set('foo', 'bar')

    def test_many_keys(self):
        k = lambda i: 'key%s' % i
        v = lambda i: '%sval' % i

        for i in range(1000):
            a0,a1 = (self.api0, self.api1) if i%2==0 else (self.api1, self.api0)
            a0.assert_set(k(i), v(i))
            a1.assert_get(k(i), v(i))

    def test_multi_threaded(self):
        def mk_key(tid, reqid):
            return '%sK%s' % (tid, reqid)

        def mk_val(tid, reqid):
            return '%s%s%s' % (tid,'V'*37, reqid)

        def read_thread_target(tid, count, local_api):
            for i in range(count):
                local_api.assert_recv_msg(mk_key(tid, i), mk_val(tid,i))

        def write_thread_target(tid, count, local_api):
            reqs = []
            for i in range(count):
                reqs.append('set %s %s' % (mk_key(tid,i), mk_val(tid,i)))
            local_api._send('\n'.join(reqs) + '\n')

        apis = []
        threads = []
        thread_pairs=8
        request_count = 10*1000

        for i in range(request_count):
            self.api0.assert_set(mk_key('main', i), mk_val('main', i))

        for i in range(thread_pairs):
            local_api = EpochAPI(LIST_ADDR, LIST_PORT)
            apis.append(local_api)
            threads.append(threading.Thread(target=read_thread_target, args=(i, request_count, local_api)))
            threads.append(threading.Thread(target=write_thread_target, args=(i, request_count, local_api)))

        map(lambda t: t.start(), threads)

        for i in range(request_count):
            self.api1.assert_get(mk_key('main', i), mk_val('main', i))

        map(lambda t: t.join(), threads)
        map(lambda a: a.send_quit(), apis)

        for tid in range(thread_pairs):
            for req in range(request_count):
                self.api0.assert_get(mk_key(tid, req), mk_val(tid, req))

if __name__ == "__main__":
    logging.basicConfig(filename='test.log', filemode='w', level=logging.DEBUG)
    unittest.main()
