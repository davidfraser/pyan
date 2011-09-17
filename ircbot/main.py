import threading
import irc

class CommandSyntaxError(Exception):
    def __init__(self, message):
        super(CommandSyntaxError, self).__init__()
        self.message = message

class Bot(irc.DumbController):
    def __init__(self, client):
        super(Bot, self).__init__()
        
        self.client = client
        self.handlers = {}
        
        self.last_message = None
        
        self.handlers['commands'] = self.commands_handler
        self.handlers['explain'] = self.explain_handler
        self.handlers['quit'] = self.quit_handler
        self.handlers['friends'] = self.friends_handler
        self.handlers['befriend'] = self.befriend_handler
        self.handlers['cookie'] = self.cookie_handler
        
        self.friends = ['edmund', 'edmund2']
    
    def see(self, channel, prefix, name):
        super(Bot, self).see(channel, prefix, name)
        
        if name in self.friends:
            self.client.speak(channel, 'Hi %s' % name)
        
    def hear(self, sender, recipient, text):
        super(Bot, self).hear(sender, recipient, text)
        
        if not text.endswith('?') or sender not in self.friends:
            return
        
        try:
            self.do_command(sender, recipient, text)
        except CommandSyntaxError, ex:
            self.client.speak(recipient, '...')
            self.last_message = ex.message
            print 'Message from handler: %s' % self.last_message
    
    def do_command(self, sender, recipient, text):
        text = text[0:len(text)-1].strip()
        if ' ' in text:
            command, text = text.split(' ', 1)
            text = text.lstrip()
        else:
            command = text
            text = ''
        if len(text) == 0:
            params = []
        else:
            params = text.split(' ')
        if command not in self.handlers:
            raise CommandSyntaxError('Unrecognised command: %s' % command)
        print 'Running command: %s' % command
        print 'Params are: %s' % params
        handler = self.handlers[command]
        handler(sender, recipient, params)
    
    def commands_handler(self, sender, recipient, params):
        if len(params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params)))
        self.client.speak(recipient, 'Commands: %s' % ', '.join(self.handlers.keys()))
    
    def explain_handler(self, sender, recipient, params):
        if len(params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params)))
        if self.last_message is None:
            raise CommandSyntaxError('No last message')
        self.client.speak(recipient, 'Last message: %s' % self.last_message)
        self.last_message = None
    
    def quit_handler(self, sender, recipient, params):
        if len(params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params)))
        self.client.speak(recipient, 'Ok')
        self.client.quit()

    def friends_handler(self, sender, recipient, params):
        if len(params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params)))
        self.client.speak(recipient, 'Friends: %s' % (', '.join(self.friends)))

    def befriend_handler(self, sender, recipient, params):
        if len(params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params[1:])))
        name = params[0]
        if name in self.friends:
            raise CommandSyntaxError('Already a friend: %s' % name)
        self.friends.append(name)
        self.client.speak(recipient, 'Ok')
    
    def cookie_handler(self, sender, recipient, params):
        if len(params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(params[1:])))
        name = params[0]
        self.client.act(recipient, 'gives cookie to %s' % name)


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
