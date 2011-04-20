import sys
import struct
import array
import math
import string
import random
import re


SAMPLE_RATE = 22050.0


class WaveFile(object):
    
    def __init__(self, sampling_rate = SAMPLE_RATE, bits_per_sample = 16, channels = 1):
        self.sampling_rate = sampling_rate
        self.bits_per_sample = bits_per_sample
        self.channels = channels
        self.data = []


    def append_sample(self, sample):
        self.data.append(sample)


    def append_note(self, note):
        note.cache()
        for s in note.render():
            v = int(s)
            if v > 32767:
                v = 32767
            elif v < -32768:
                v = -32768
            self.data.append(v)


    def render(self):

        data_size = len(self.data) * (self.bits_per_sample/8) * self.channels
        data_rate = self.channels*self.bits_per_sample*self.sampling_rate/8
        block_align = self.channels*self.bits_per_sample/8
        data = array.array('h', self.data).tostring()
        result = struct.pack('4sL4s4sLHHLLHH4sL', 'RIFF', data_size + 44, 'WAVE', 'fmt ',
                16, 1, self.channels, int(self.sampling_rate), int(data_rate), block_align,
                self.bits_per_sample, 'data', data_size)
        return result + data


class Note(object):

    def generate(self):
        raise NotImplementedError


    def render(self):
        self.cache()
        return self.cached_data


    def cache(self):
        if hasattr(self, 'cached_data'):
            return False
        self.cached_data = self.generate()
        self.length = len(self.cached_data)
        return True


    def save(self, filename='music.wav'):
        wf = WaveFile()
        wf.append_note(self)
        data = wf.render()
        f = open(filename, 'wb')
        f.write(data)
        f.close()
        return filename


    def play(self):
        filename = self.save()
        try:
            import winsound
            winsound.PlaySound(filename, winsound.SND_FILENAME)
        except ImportError:
            print 'winsound module not found, try playing music.wav normally'


class PureTone(Note):

    def __init__(self, pitch = 440, volume = 0.5, duration = 0.25):
        self.pitch = pitch
        self.volume = volume
        self.duration = duration
        self.length = duration
        self.name = '@%d' % pitch


    def generate(self):
        result = []
        #ra = random.random() * math.pi * 2 / 100
        ra = 0
        f1 = self.volume*32767.0
        f2 = math.pi*2/SAMPLE_RATE * self.pitch
        sin = math.sin
        for i in xrange(int(self.duration*SAMPLE_RATE)):
            result.append(f1 * sin(ra + i * f2))
        return result


class HarmonicTone(Note):

    def __init__(self, underlying, harmonics = [1.0]):
        if not isinstance(underlying, PureTone):
            raise TypeError
        self.underlying = underlying
        self.duration = underlying.duration
        self.harmonics = harmonics
        self.length = underlying.length
        self.name = 'H' + underlying.name


    def generate(self):
        f = 1
        notes = []
        for h in self.harmonics:
            pitch = self.underlying.pitch * f
            volume = self.underlying.volume * h
            n = PureTone(pitch, volume, self.duration)
            n.render()
            notes.append(n)
            f += 1
        
        result = []
        
        for i in xrange(int(self.duration*SAMPLE_RATE)):
            v = 0.0
            for n in notes:
                v += n.cached_data[i]
            result.append(v)
        
        return result


class DecayNote(Note):

    def __init__(self, underlying, decay=0.999999, initial=1.0):
        self.underlying = underlying
        self.decay = decay
        self.initial = initial
        self.length = 0
        self.name = 'D' + underlying.name


    def generate(self):
        result = []
        f = self.initial
        self.underlying.duration = 1
        for s in self.underlying.render():
            result.append(s*f)
            f *= self.decay
            if f < 0.001:
                break
        return result


class EnvelopeNote(Note):

    def __init__(self, underlying, envelope, duration=None, decay=0.9999):
        self.underlying = underlying
        self.envelope = envelope
        self.length = underlying.length
        self.name = 'E' + underlying.name
        self.duration = duration
        self.decay = decay


    def generate(self):
        result = []
        self.underlying.duration = self.duration+5
        prev = 0
        next = 1
        width = self.envelope[next][0]*SAMPLE_RATE - self.envelope[prev][0]*SAMPLE_RATE
        i = 0
        for s in self.underlying.render():
            if i > self.duration*SAMPLE_RATE:
                f *= self.decay
                if f < 0.01:
                    break
            elif next >= len(self.envelope):
                pass
            else:
                if i >= self.envelope[next][0]*SAMPLE_RATE:
                    next += 1
                    prev += 1
                    if next >= len(self.envelope):
                        result.append(s*f)
                        i += 1
                        continue
                    width = self.envelope[next][0]*SAMPLE_RATE - self.envelope[prev][0]*SAMPLE_RATE
                f1 = self.envelope[prev][1] * (1 - (i - self.envelope[prev][0]*SAMPLE_RATE)/width)
                f2 = self.envelope[next][1] * (1 - (self.envelope[next][0]*SAMPLE_RATE - i)/width)
                f = f1+f2
            result.append(s*f)
            i += 1
        self.duration = i/SAMPLE_RATE
        self.length = i/SAMPLE_RATE
        return result


class Silence(Note):

    def __init__(self, duration=0.25):
        self.duration = duration
        self.length = duration


    def generate(self):
        return []


class NoteMixture(Note):

    def __init__(self):
        self.list = []
        self.length = 0


    def append_note(self, note, offset=None):
        if offset is None:
            offset = self.length
        self.list.append((offset, note))
        self.list.sort()
        self.length = max(self.length, offset + note.length)


    def generate(self):
        print 'Caching:',
        for offset,note in self.list:
            if note.cache():
                print note.name,
                sys.stdout.flush()
        print
        
        print 'Queueing:',
        result = []
        active_notes = {}
        next_note = 0
        i = 0
        while next_note < len(self.list):
            next_stop = self.list[next_note][0]
            while i < next_stop:
                v = 0
                for id in list(active_notes):
                    if active_notes[id] >= self.list[id][1].length:
                        del active_notes[id]
                        print '%0.2f-%s' % (i/SAMPLE_RATE, self.list[id][1].name),
                        sys.stdout.flush()
                        continue
                    v = v + self.list[id][1].cached_data[active_notes[id]]/len(active_notes)
                    active_notes[id] += 1
                result.append(v)
                i += 1
            active_notes[next_note] = 0
            print '%0.2f+%s' % (i/SAMPLE_RATE, self.list[next_note][1].name),
            next_note += 1
        
        while active_notes != {}:
            v = 0
            for id in list(active_notes):
                if active_notes[id] >= self.list[id][1].length:
                    del active_notes[id]
                    print '%0.2f-%s' % (i/SAMPLE_RATE, self.list[id][1].name),
                    continue
                v = v + self.list[id][1].cached_data[active_notes[id]] #/len(active_notes)
                active_notes[id] += 1
            result.append(v)
            i += 1
        print
        
        print 'Playing %d notes...' % len(self.list)
        sys.stdout.flush()
        return result
    
    
    def collapse(self):
        new_mix = NoteMixture()
        for offset,note in self.list:
            if isinstance(note, NoteMixture):
                for offset2,note2 in note.collapse().list:
                    new_mix.append_note(note2, offset+offset2)
            elif isinstance(note, Silence):
                pass
            else:
                new_mix.append_note(note, offset)
        
        return new_mix


    def trim(self):
        new_mix = NoteMixture()
        adjust = -self.list[0][0]
        for offset,note in self.list:
            new_mix.append_note(note, offset+adjust)
        return new_mix


def could_be_note(n):
    return re.match('^[a-g][sfn]?$', n)


class Sequence(object):

    NOTES = {
        'a': 0,
        'b': 2,
        'c': 3,
        'd': 5,
        'e': 7,
        'f': 8,
        'g': 10,
    }
    
    def __init__(self):
        self.tempo = 1
        self.sharpness = {}
        for n in self.NOTES:
            self.apply_sharpness(n, 'n')
        self.list = []


    def __getattr__(self, name):
        if could_be_note(name):
            note_name = name[0]
            suffix = name[1:]
            self.append_note(note_name, suffix)
            return self
        else:
            raise AttributeError


    def apply_sharpness(self, name, sharpness):
        if sharpness in ['s', 'f', 'n']:
            self.sharpness[name]  = sharpness


    def get_note(self, name):
        pos = self.NOTES[name]
        sharpness = self.sharpness[name]
        if sharpness == 's':
            pos += 1
        elif sharpness == 'f':
            pos -= 1
        
        octave = pos / 12
        index = pos % 12
        map = {
            0: 'A',
            1: 'A#',
            2: 'B',
            3: 'C',
            4: 'C#',
            5: 'D',
            6: 'D#',
            7: 'E',
            8: 'F',
            9: 'F#',
            10: 'G',
            11: 'G#',
        }
        return '%s%d' % (map[index], octave)


    def append_note(self, name, suffix):
        self.apply_sharpness(name, suffix)
        note = self.get_note(name)
        self.list.append(note)


class Instrument(object):
    def __init__(self):
        pass

    def make_note(self, semitone, duration):
        c = PureTone(pitch=440 * 2**(semitone/12.0), duration=duration)
        #organ = [0.5,0.0,0.0,0.0,0.5,0.0,0.0,0.0,0.5,0.0,0.0,0.0,0.5]
        organ = [0.5]
        organ_envelope = [(0.0, 0.0), (0.05, .75), (0.3, 0.75), (2.0, 0.5)]
        piano = [0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5,0.5]
        piano_envelope = [(0.0, 0.0), (0.05, 0.75), (0.2, 1.00), (0.7, 0.5), (1.0, 0.25), (2.0, 0.25)]
        c2 = HarmonicTone(c, organ)
        c3 = EnvelopeNote(c2, organ_envelope, duration=duration, decay=0.9999)
        return c3


def make_piano_note(note, octave=0, duration=2, decay=0.99977):
    
    map = {
        'A': 0,
        'B': 2,
        'C': 3,
        'D': 5,
        'E': 7,
        'F': 8,
        'G': 10
    }
    suffix = note[1:]
    note = note[0].upper()
    
    if note not in map or suffix not in ['#', 'b', '']:
        raise ValueError(note+suffix)
    
    semitone = map[note]
    if suffix == '#':
        semitone += 1
    elif suffix == 'b':
        semitone -= 1
    
    semitone += 12*octave
    
    piano = Instrument()
    n = piano.make_note(semitone, duration)
    n.name = note + suffix
    if octave != 0:
        n.name += str(octave)
    n.length = duration
    return n


note_cache = {}

def make_piano_melody(melody, duration=0.25):

    notes = melody.split()
    if notes == []:
        note = Silence(duration=duration)
        return note
    
    mix = NoteMixture()
    offset = 0
    for n in notes:
        if n == ',':
            try:
                note = note_cache[(',', duration)]
            except KeyError:
                note = Silence(duration=duration)
                note_cache[(',', duration)] = note
        elif n == 'T<':
            duration *= 0.5
            continue
        elif n == 'T>':
            duration *= 2.0
            continue
        elif n == '+':
            offset -= SAMPLE_RATE*duration
            continue
        else:
            octave = 0
            if '0' <= n[-1] <= '9':
                i = 1
                if n[i] in ['#', 'b']:
                    i += 1
                octave = int(n[i:])
                n = n[:i]
            try:
                note = note_cache[(n, octave, duration)]
            except KeyError:
                note = make_piano_note(n, octave, duration*0.5)
                note_cache[(n, octave, duration)] = note
        mix.append_note(note, offset=offset)
        offset += SAMPLE_RATE*duration
    mix.length = offset
    return mix


def mary():
    print Sequence().e.d.c.d.e.e.e.list
    n = make_piano_melody('E D C D E E E , D D D , E G G , E D C D E E E E D D E D C')
    return n.collapse().trim()


def beethoven():
    m1 = make_piano_melody("""G#-1 C# E G#-1 C# E G#-1 C# E G#-1 C# E
    G#-1 C# E G#-1 C# E G#-1 C# E G#-1 C# E
    A C# E A C# E A D F# A D F#
    G#-1 C# F# G#-1 C# E G#-1 C# D# G#-1 C D#""")
    m2 = make_piano_melody("""C#-1 ,
    B-1 ,
    A-1   F#-2
    G#-2   G#-2""", duration=1.5, decay=0.99995)
    m3 = make_piano_melody("""C#-2 ,
    B-2 ,
    A-2   F#-3
    G#-3   G#-3""", duration=1.5, decay=0.99995)

    n = NoteMixture()
    n.append_note(m1, offset=0)
    n.append_note(m2, offset=0)
    n.append_note(m3, offset=0)
    return n.collapse().trim()


def demo():
    n1 = make_piano_melody('C D Eb F G Ab1 B1 T> C1 T< B1 Ab1 G F Eb D T> C', duration=1/6.0)

    n2 = NoteMixture()
    n2.append_note(make_piano_note('C', decay=0.9998))
    n2.append_note(make_piano_note('Eb', decay=0.9998))
    n2.append_note(make_piano_note('G', decay=0.9998))
    n2.append_note(make_piano_note('C', octave=1, decay=0.9998))
    
    n = NoteMixture()
    n.append_note(n1)
    n.append_note(n2, offset=18*1/6.0*SAMPLE_RATE)
    return n.collapse().trim()


def nimphes_des_bois():
    minim_length=4/6.0
    bar_length = minim_length*4
    
    parts = [[]]*5
    
    #0
    parts[0].append("""
T> F#1 A2 T< B2 C#2 D2
""")

    n = NoteMixture()
    for series in parts:
        mix = NoteMixture()
        for line in series:
            line_note = make_piano_melody(line, duration=minim_length)
            minim_count = line_note.length/SAMPLE_RATE/minim_length
            if minim_count % 4 != 0:
                print "Warning: bar length is: %d minims" % minim_count
            mix.append_note(line_note)
        n.append_note(mix, offset=0)
    
    return n.collapse().trim()


def test():
    note1 = make_piano_note('C', 0)
    note1.play()


try:
    import psyco
    psyco.full()
    #print 'Optimised'
except ImportError:
    #print 'Not optimised'
    pass


if __name__ == '__main__':
    #demo().play()
    test()
