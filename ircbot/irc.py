import sys
import socket
import irc_replies

DEFAULT_PORT = 6667

BUFFER_SIZE = 1024

MAX_MESSAGE = 510

def warn(s):
    print >>sys.stderr, s

def notice(s):
    print >>sys.stderr, s

class Message(object):
    def __init__(self, prefix, command, params=None):
        if params is None:
            params = []
        elif type(params) is str:
            params = [params]
        
        self.prefix = prefix
        self.command = command
        self.params = params
    
    def __eq__(self, other):
        if self.prefix != other.prefix:
            return False
        if self.command != other.command:
            return False
        if len(self.params) != len(other.params):
            return False
        for sp,op in zip(self.params, other.params):
            if sp != op:
                return False
        return True
    
    def __ne__(self, other):
        return not (self == other)
        
    def __str__(self):
        return '[%s %s [%s]]' % (self.prefix, self.command, ' '.join(["'%s'" % p for p in self.params]))


class Connection(object):
    def __init__(self, addr=None):
        if addr is None:
            return
        
        if ':' in addr:
            self.host, self.port = addr.split(':')
        else:
            self.host, self.port = addr, DEFAULT_PORT
        
        self.socket = None
        self.buffer = ''

    def open(self):
        s = None
        for res in socket.getaddrinfo(self.host, self.port, socket.AF_UNSPEC, socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res
            try:
                s = socket.socket(af, socktype, proto)
            except socket.error, msg:
                s = None
                continue
            
            try:
                s.connect(sa)
            except socket.error, msg:
                s.close()
                s = None
                continue
            
            break
        
        if s is None:
            raise Exception('Unable to connect to %s:%d' % (self.host, self.port))
        
        s.setblocking(True)
        self.socket = s
    
    def close(self):
        if self.socket is not None:
            self.socket.close()
            self.socket = None
    
    def read(self):
        while self.socket is not None and '\n' not in self.buffer:
            input = self.socket.recv(BUFFER_SIZE)
            if len(input) == 0:
                self.close()
                break
            #print 'received "%s"' % input
            self.buffer = self.buffer + input.replace('\r\n', '\n')
        
        if '\n' in self.buffer:
            line,self.buffer = self.buffer.split('\n', 1)
        else:
            warn('No CRLF in buffer')
            line = self.buffer
        
        message = self.decode(line)
        return message
    
    def send(self, message):
        if type(message) is str:
            line = message
        else:
            notice('Out: %s' % message)
            line = self.encode(message)
        self.socket.send(line)
        #print 'sent "%s"' % line
    
    def decode(self, line):
        if len(line) == 0:
            warn('Empty message')
            return None
        
        # Read optional prefix
        if line[0] == ':':
            prefix,line = line[1:].split(' ', 1)
        else:
            prefix = None
        line = line.lstrip()
        
        # Read command
        if ' ' in line:
            command,line = line.split(' ', 1)
            line = line.lstrip()
        else:
            warn('No command')
            command = None
        
        # Read params
        params = []
        while len(line) > 0 and line[0] != ':':
            if ' ' in line:
                param,line = line.split(' ', 1)
            else:
                param = line
                line = ''
            params.append(param)
            line = line.lstrip()
            
        if len(line) > 0 and line[0] == ':':
            params.append(line[1:])
        else:
            warn('No trailing parameter')
        
        # Translate command
        try:
            num = int(command)
            name = irc_replies.replies[num]
            command = name
        except ValueError:
            pass
        except KeyError:
            pass
        
        message = Message(prefix, command, params)
        notice('In: %s' % message)
        return message
    
    def encode(self, message):
        line = ''
        if message.prefix is not None:
            line = ':%s ' % message.prefix
        line += ' %s' % message.command
        for i in range(len(message.params)):
            if i == len(message.params) - 1:
                line += ' :%s' % message.params[i]
            else:
                line += ' %s' % message.params[i]
        if len(line) > MAX_MESSAGE:
            warn('Message length is %d, truncating!' % len(line))
            line = line[:MAX_MESSAGE]
        
        return line + '\r\n'

STATE_START, STATE_OPEN, STATE_REGISTER, STATE_RUN, STATE_QUIT, STATE_CLOSE = range(6)

class DumbController(object):
    def __init__(self):
        pass
    
    def see(self, channel, prefix, name):
        print 'See %s in %s' % (name, channel)
        
    def hear(self, sender, recipient, text):
        print '%s says to %s, "%s"' % (sender, recipient, text)


class Client(object):
    
    def __init__(self):
        self.connection = None
        self.nick = None
        self.hostname = None
        self.servername = None
        self.realname = None
        self.state = STATE_START
        self.channels = {}
        self.send_queue = []
        self.controller = DumbController()
    
    def connect(self, addr):
        conn = Connection(addr)
        conn.open()
        self.conn = conn
        self.state = STATE_OPEN
    
    def disconnect(self):
        self.conn.close()
        self.state = STATE_CLOSE
    
    def send(self, message):
        self.send_queue.append(message)
    
    def update(self):
        if self.state == STATE_OPEN:
            nick = Message(None, 'NICK', [self.nick])
            self.conn.send(nick)
        
            user = Message(None, 'USER', [self.nick, self.hostname, self.servername, self.realname])
            self.conn.send(user)
            
            self.state = STATE_REGISTER
        
        if self.state == STATE_RUN:
            while len(self.send_queue) > 0:
                message = self.send_queue.pop(0)
                self.conn.send(message)
        
        message = self.conn.read()
        if message is not None:
            self.process_message(message)
        
        if self.conn.socket is None:
            self.disconnect()
    
    def process_message(self, message):
        if self.state == STATE_REGISTER and message.command != 'NOTICE':
            self.state = STATE_RUN
        
        if message.command == 'RPL_TOPIC':
            channel = message.params[-2]
            self.channels[channel] = message.params[-1]
        elif message.command == 'RPL_NAMREPLY':
            channel = message.params[-2]
            self.channels[channel] = None
            for name in message.params[-1].split(' '):
                if name[0] in ['@', '+']:
                    prefix = name[0]
                    name = name[1:]
                else:
                    prefix = ''
                self.controller.see(channel, prefix, name)
        elif message.command == 'PRIVMSG':
            name = message.prefix
            if '!' in name:
                name = name.split('!', 1)[0]
            recipient = message.params[0]
            text = message.params[-1]
            self.controller.hear(name, recipient, text)
        elif message.command == 'JOIN':
            name = message.prefix
            if '!' in name:
                name = name.split('!', 1)[0]
            channel = message.params[-1]
            self.controller.see(channel, '', name)
    
    def speak(self, channel, text):
        privmsg = Message(None, 'PRIVMSG', [channel, text])
        self.send(privmsg)
    
    def join(self, channel):
        join = Message(None, 'JOIN', [channel])
        self.send(join)
    
    def quit(self):
        quit = Message(None, 'QUIT', ['Bye!'])
        self.conn.send(quit)
        self.state = STATE_QUIT
    
    def run(self):
        while self.state != STATE_CLOSE:
            self.update()


def test_decode(line, expected):
    c = Connection()
    message = c.decode(line)
    if message != expected:
        print >>sys.stderr, 'Test failed: "%s" decoded to %s instead of %s' % (line, message, expected)

    
def test():
    test_decode(':prefix command :trailing param', Message('prefix', 'command', ['trailing param']))
    test_decode('command :trailing param', Message(None, 'command', ['trailing param']))
    test_decode('123 :trailing param', Message(None, '123', ['trailing param']))
    test_decode('command param :trailing param', Message(None, 'command', ['param', 'trailing param']))
    test_decode('command  param   :trailing param', Message(None, 'command', ['param', 'trailing param']))


if __name__ == '__main__':
    test()
