import threading
import irc

class Bot(irc.DumbController):
    def __init__(self, client):
        irc.DumbController.__init__(self)
        self.client = client
    
    def see(self, channel, prefix, name):
        irc.DumbController.see(self, channel, prefix, name)
        if name in ['edmund', 'edmund2']:
            self.client.speak(channel, 'Hi %s' % name)
        
    def hear(self, sender, recipient, text):
        irc.DumbController.hear(self, sender, recipient, text)
        if text == 'die':
            self.client.quit()


client = irc.Client()
client.nick = 'rhobot'
client.hostname = 'smaug'
client.servername = 'smaug'
client.realname = 'Rho Bot'
client.controller = Bot(client)
client.connect('irc.freenode.net:6667')
thread = threading.Thread(target=client.run)
thread.start()
client.join('##newzealand')
thread.join()
