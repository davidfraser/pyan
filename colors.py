import math


class Colorizer(object):
    def __init__(self, colored, logger):
        self.colored = colored
        self.logger = logger
        self.hues = map(lambda d: d/360., [0, 120, 50, 190, 90, 240, 0, 300])
        self.top_ns_to_hue_idx = {}
        self.cidx = 0  # first free hue index

    def get_hue_idx(self, node):
        ns = node.get_toplevel_namespace()
        self.logger.info(
                "Coloring %s (top-level namespace %s)"
                % (node.get_short_name(), ns))
        if ns not in self.top_ns_to_hue_idx:  # not seen yet
            self.top_ns_to_hue_idx[ns] = self.cidx
            self.cidx += 1
            if self.cidx >= len(self.hues):
                self.logger.warn(
                    "WARNING: too many top-level namespaces;"
                    "colors wrapped")
                self.cidx = 0  # wrap around
        return self.top_ns_to_hue_idx[ns]

    def make_colors(self, n):
            idx = self.get_hue_idx(n)
            if self.colored:
                H = self.hues[idx]
                S = 1.0
                L = max([1.0 - 0.1*n.get_level(), 0.1])
                A = 0.7  # make nodes translucent (to handle possible overlaps)
                fill_RGBA = list(self.hsl2rgb(H, S, L))
                fill_RGBA.append(A)
                fill_RGBA = self.htmlize_rgb(*fill_RGBA)

                if L >= 0.3:
                    # black text on light nodes
                    text_RGB = self.htmlize_rgb(0.0, 0.0, 0.0)
                else:
                    # white text on dark nodes
                    text_RGB = self.htmlize_rgb(1.0, 1.0, 1.0)
            else:
                fill_RGBA = self.htmlize_rgb(1.0, 1.0, 1.0, 0.7)
                text_RGB = "#000000"

            return fill_RGBA, text_RGB, idx

    @staticmethod
    def hsl2rgb(*args):
        """Convert HSL color tuple to RGB.

        Parameters:  H,S,L, where
            H,S,L = HSL values as double-precision floats, with each component
            in [0,1].

        Return value:
            R, G, B tuple

        For more information:
            https://en.wikipedia.org/wiki/HSL_and_HSV#From_HSL

        """
        if len(args) != 3:
            raise ValueError(
                    "hsl2rgb requires exactly 3 arguments. See docstring.")

        H = args[0]
        S = args[1]
        L = args[2]

        if H < 0.0 or H > 1.0:
            raise ValueError("H component = %g out of range [0,1]" % H)
        if S < 0.0 or S > 1.0:
            raise ValueError("S component = %g out of range [0,1]" % S)
        if L < 0.0 or L > 1.0:
            raise ValueError("L component = %g out of range [0,1]" % L)

        # hue chunk
        Hpf = H / (60./360.)  # "H prime, float" (H', float)
        Hp = int(Hpf)  # "H prime" (H', int)
        if Hp >= 6:  # catch special case 360deg = 0deg
            Hp = 0

        C = (1.0 - math.fabs(2.0*L - 1.0))*S  # HSL chroma
        X = C * (1.0 - math.fabs(math.modf(Hpf / 2.0)[0] - 1.0))

        if S == 0.0:  # H undefined if S == 0
            R1, G1, B1 = (0.0, 0.0, 0.0)
        elif Hp == 0:
            R1, G1, B1 = (C,   X,   0.0)
        elif Hp == 1:
            R1, G1, B1 = (X,   C,   0.0)
        elif Hp == 2:
            R1, G1, B1 = (0.0, C,   X)
        elif Hp == 3:
            R1, G1, B1 = (0.0, X,   C)
        elif Hp == 4:
            R1, G1, B1 = (X,   0.0, C)
        elif Hp == 5:
            R1, G1, B1 = (C,   0.0, X)

        # match the HSL Lightness
        #
        m = L - 0.5*C
        R, G, B = (R1 + m, G1 + m, B1 + m)

        return R, G, B

    @staticmethod
    def htmlize_rgb(*args):
        """HTML-ize an RGB(A) color.

        Parameters:  R, G, B[,alpha], where
            R, G, B = RGB values as double-precision floats, with each
            component in [0,1].
            alpha = optional alpha component for translucency, in [0,1].
            (1.0 = opaque)

        Example:
            htmlize_rgb(1.0, 0.5, 0)       =>  "#FF8000"    (RGB)
            htmlize_rgb(1.0, 0.5, 0, 0.5)  =>  "#FF800080"  (RGBA)

        """
        if len(args) < 3:
            raise ValueError(
                    "htmlize_rgb requires 3 or 4 arguments. See docstring.")

        R = args[0]
        G = args[1]
        B = args[2]

        if R < 0.0 or R > 1.0:
            raise ValueError("R component = %g out of range [0,1]" % R)
        if G < 0.0 or G > 1.0:
            raise ValueError("G component = %g out of range [0,1]" % G)
        if B < 0.0 or B > 1.0:
            raise ValueError("B component = %g out of range [0,1]" % B)

        R = int(255.0*R)
        G = int(255.0*G)
        B = int(255.0*B)

        if len(args) > 3:
            alp = args[3]
            if alp < 0.0 or alp > 1.0:
                raise ValueError("alpha component = %g out of range [0,1]"
                                 % alp)
            alp = int(255.0*alp)
            make_RGBA = True
        else:
            make_RGBA = False

        if make_RGBA:
            return "#%02X%02X%02X%02X" % (R, G, B, alp)
        else:
            return "#%02X%02X%02X" % (R, G, B)
