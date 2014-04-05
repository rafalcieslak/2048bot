#!/usr/bin/python2

import sys
from threading import Thread
from time import sleep

# This value determines the time (in secs) the bot sleeps before moves. That break is nesessary, because
# the game is animated, and the bot is not able to determine current game state before the animation
# finishes. If you are using a  custom game version e.g. with animations disabled, you can adjust this value
# to your wish. Decreasing it will increase bot speed.
sleeptime = 0.4

# This constant determines the serch depth the core algorithm will perform. The greater this value, the more
# accurate predictions are, and therefore the bot is more likely to win. However, if the value is too big,
# the bot wil start running incredibelly slow. You may want to decrease this value on a slow computer.
# Recommended value for decent CPUs: 7.
depth = 7

try:
    from pymouse import PyMouseEvent
    from pykeyboard import PyKeyboard
except ImportError:
    print "PyUserInput seems to be missing. Please install it (or place in this directory) to run the bot."
    print "You can get PyUserInput from https://github.com/SavinaRoja/PyUserInput"
    sys.exit(1)

try:
    import bot_core
except ImportError:
    print "The bot_core is missing. Please build the module before running this script."
    print "To build the module, enter the bot_core directory, and run:"
    print "   python2 setup.py build"
    print "and copy the bot_core.so (or bot_core.dll) file from bot_core/build/lib.YOUR_ARCH/"
    print "directory into this one (symlink is also fine)."
    print ""
    print "On Linux systems, you can simply run "
    print "   ./build_core-linux.sh"
    print "which will do the above."
    sys.exit(1)

try:
    import gtk.gdk
except ImportError:
    print "It seems that GDK/GTK is missing. Is pygtk installed?"
    print "The bot needs GTK in order to have a look at the screen without requesting time-costly printscreens."
    sys.exit(1)

# A dictionary and a set of procedures for recognising tile types from their color.
colors_dict = {}
def add_color_to_dict(color,tile):
    colors_dict[(color[0]/4,color[1]/4,color[2]/4)] = tile
def prepare_colors_dict():
    add_color_to_dict((205, 192, 180), 0) # empty
    add_color_to_dict((205, 192, 176), 0) # also empty
    add_color_to_dict((238, 228, 218), 1) # 2
    add_color_to_dict((237, 224, 200), 2) # 4
    add_color_to_dict((242, 177, 121), 3) # 8
    add_color_to_dict((245, 149, 99 ), 4)
    add_color_to_dict((246, 124, 95 ), 5)
    add_color_to_dict((246, 94 , 59 ), 6) # 64
    add_color_to_dict((237, 207, 114), 7)
    add_color_to_dict((237, 204, 97 ), 8) # 256
    add_color_to_dict((236, 200, 80 ), 9)
    add_color_to_dict((237, 197, 63 ), 10)
    add_color_to_dict((236, 192, 44 ), 11) # 2048!
    add_color_to_dict((60 , 56 , 48 ),    12)
def recognize_color(color):
    try:
        return colors_dict[(color[0]/4,color[1]/4,color[2]/4)]
    except KeyError:
        print "Unable to recognize game board state!"
        print "Refer to README file for details and information on how to deal with this problem."
        sys.exit(2)

# This class is used to react on user clicks.
class ClickDetector(PyMouseEvent):
    def __init__(self):
        PyMouseEvent.__init__(self)
        self.phase = 0
        print "The bot needs to know where the game board is."
        print "Please mark it by pressing LMB somewhere inside game board border's *upper left corner*."

    def click(self, x, y, button, press):
        if button == 1 and press:
            if self.phase == 0:
                # First click.
                self.first_point = (x,y)
                self.phase = 1
                print "Thanks! Now, please press LMB somewhere inside game board border's *lower right corner*."
                print "This way the bot will learn about game field size."
                return
            elif self.phase == 1: 
                # Second click.
                self.phase = 2
                print "Great! The bot is starting now."
                self.thread = Thread(target = bot.main_loop, args=(self.first_point, (x,y))) # Run the bot!
                self.thread.start()
                return
        if press and (button != 1 or self.phase == 2):  # If this not an LMB click, or if this is a third click, cancel execution.
            print "Stopping the bot."
            self.stop()
            bot.quit_loop()
            if self.thread.is_alive():
                self.thread.join()
            sys.exit(0)

class Bot():
    def __init__(self):
        # Prepare to take control over user keyboard
        self.keyboard = PyKeyboard()
        # Prepare for getting pixel colors from the screen
        self.gdk_root_window = gtk.gdk.get_default_root_window()
        self.gdk_screen_pixbuf = gtk.gdk.Pixbuf(gtk.gdk.COLORSPACE_RGB, False, 8, 1, 1)
        # Prepare color recognition
        prepare_colors_dict()

    def print_state(self,s):
        # Displays game board as a table.
        for y in range(0,4):
            for x in range(0,4):
                sys.stdout.write( str(s[y][x]) + " ")
            print ""

    def encode_state(self,state):
        # Encodes a game board state into a 64-bit integer.
        r = 0;
        for i in range(0,16):
            r += state[i/4][i%4]
            r *= 16 if i < 15 else 1
        return r
        
    def get_pixel_color_from_screen(self,q):
        # Ask GDK about the color...
        self.gdk_screen_pixbuf = self.gdk_screen_pixbuf.get_from_drawable(self.gdk_root_window, self.gdk_root_window.get_colormap(), q[0], q[1], 0, 0, 1, 1)
        return tuple(self.gdk_screen_pixbuf.pixel_array[0, 0])

    def main_loop(self,a,b):
        # Runs the bot in a loop, assuming the game field is displayed on the screen at the coordinates from point a to b.

        # Generate a list of 16 coordinates which will be used to check for tile's color.
        probing_points = [] 
        xsize = b[0]-a[0] # Game board size
        ysize = b[1]-a[1]
        xstep = xsize/4 # Pixels from a tile to tile
        ystep = ysize/4
        xoffset = xstep/2 # Offset of the probing point from tile's corner
        yoffset = ystep/4 # sic! Do not probe the center, the numbers cover the color
        for y in range(0,4):
            for x in range(0,4):
                probing_points.append( (a[0] + x*xstep + xoffset, a[1] + y*ystep + yoffset) )

        self.loop = True
        while(self.loop):
            print "---"

            # Look at the screen and determine what the current board state is.
            board_state = []
            for y in range(0,4):
                board_state.append([])
                for x in range(0,4):
                    pixel_color = self.get_pixel_color_from_screen(probing_points[y*4 + x])
                    board_state[y].append(recognize_color( pixel_color ))

            # Display current board situation
            self.print_state(board_state)

            # Ask the core algorithm for a move recommendation
            recommended_move = bot_core.run(self.encode_state(board_state), depth)

            # Perform the move, by pressing appropriate key on the keyboard.
            movenames = {0:"UP", 1:"DOWN", 2:"LEFT", 3:"RIGHT"}
            print "Moving " + movenames[recommended_move]

            if recommended_move == 0:
                self.keyboard.tap_key(self.keyboard.up_key)
            if recommended_move == 1:
                self.keyboard.tap_key(self.keyboard.down_key)
            if recommended_move == 2:
                self.keyboard.tap_key(self.keyboard.left_key)
            if recommended_move == 3:
                self.keyboard.tap_key(self.keyboard.right_key)

            # Wait for game animation to complete before lookin at the screen again.
            sleep( sleeptime )

    def quit_loop(self):
        # Breaks the main loop.
        self.loop = False

bot = Bot()
clicks  = ClickDetector()
clicks.run()