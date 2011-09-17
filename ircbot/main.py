import threading
import irc

class Bot(object):
    def __init__(self):
        self.clients = {}
    
    def run(self):
        pass

bot = Bot()
client = irc.Client()
client.nick = 'edmundbot'
client.hostname = 'smaug'
client.servername = 'smaug'
client.realname = 'Edmund Bot'
client.connect('irc.freenode.net:6667')
thread = threading.Thread(target=client.run)
thread.start()
client.join('#edmundbot')
thread.join()
