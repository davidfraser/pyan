import threading
import irc

import nl

class CommandSyntaxError(Exception):
    def __init__(self, message):
        super(CommandSyntaxError, self).__init__()
        self.message = message

class Context(object):
    """The context of a message in an IRC channel, that the Bot or a Handler might want to respond to."""

    def __init__(self, bot, client, channel=None, nick=None):
        self.bot = bot
        self.client = client
        self.channel = channel
        self.nick = nick

    def speak(self, text):
        """Speak in reply to whomever or whatever originated the message."""
        
        if self.channel is not None:
            recipient = self.channel
        else:
            recipient = self.nick
        self.client.speak(recipient, text)

    def act(self, action):
        """Act in response to whomever or whatever originated the message."""
        
        if self.channel is not None:
            recipient = self.channel
        else:
            recipient = self.nick
        self.client.act(recipient, action)


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
        self.handlers['join'] = self.join_handler
        self.handlers['part'] = self.part_handler
        
        self.friends = ['edmund', 'edmund2', 'edmund3']
    
    def see(self, channel, prefix, name):
        super(Bot, self).see(channel, prefix, name)
        
        if name in self.friends:
            #self.client.speak(channel, 'Hi %s' % name)
            pass
        
    def hear(self, sender, recipient, text):
        super(Bot, self).hear(sender, recipient, text)
        
        context = Context(self, self.client)
        if recipient[0] == '#':
            context.channel = recipient
        context.nick = sender
        context.text = text
        
        try:
            self.do_ambient(context)
        except Exception, ex:
            print ex
        
        if not text.endswith('?') or sender not in self.friends:
            return
        
        try:
            self.do_command(context)
        except CommandSyntaxError, ex:
            context.speak('...')
            self.last_message = ex.message
            print 'Message from handler: %s' % self.last_message

    def do_ambient(self, context):
        new_words = nl.parse_sentence(context.text, True)
        if len(new_words) > 0:
            #context.speak('New words: %s' % (' '.join([nl.annotation(x) for x in new_words])))
            pass

    def do_command(self, context):
        text = context.text
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
        
        context.command = command
        context.params = params
        
        handler(context)
    
    def commands_handler(self, context):
        if len(context.params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params)))
        context.speak('Commands: %s' % ', '.join(self.handlers.keys()))
    
    def explain_handler(self, context):
        if len(context.params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params)))
        if self.last_message is None:
            raise CommandSyntaxError('No last message')
        context.speak('Last message: %s' % self.last_message)
        self.last_message = None
    
    def quit_handler(self, context):
        if len(context.params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params)))
        context.speak('Ok')
        context.client.quit()

    def friends_handler(self, context):
        if len(context.params) > 0:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params)))
        context.speak('Friends: %s' % (', '.join(self.friends)))

    def befriend_handler(self, context):
        if len(context.params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(context.params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params[1:])))
        name = context.params[0]
        if name in self.friends:
            raise CommandSyntaxError('Already a friend: %s' % name)
        self.friends.append(name)
        context.speak('Ok')
    
    def cookie_handler(self, context):
        if len(context.params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(context.params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params[1:])))
        name = context.params[0]
        context.act('gives cookie to %s' % name)
    
    def join_handler(self, context):
        if len(context.params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(context.params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params[1:])))
        channel = context.params[0]
        context.client.join(channel)
        context.speak('Ok')
    
    def part_handler(self, context):        
        if len(context.params) < 1:
            raise CommandSyntaxError('Need a param')
        if len(context.params) > 1:
            raise CommandSyntaxError('Superfluous params: %s' % (' '.join(context.params[1:])))
        name = context.params[0]
        self.client.part(name)
        context.speak('Ok')


THREADING = False

client = irc.Client()
client.nick = 'skynetbot'
client.hostname = 'smaug'
client.servername = 'smaug'
client.realname = 'Skynet'
client.controller = Bot(client)
client.connect('irc.freenode.net:6667')
client.join('##newzealand')
if THREADING:
    thread = threading.Thread(target=client.run)
    thread.start()
    thread.join()
else:
    client.run()

