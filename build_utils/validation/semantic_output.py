class ColorDummy():
    class Empty(object):
        def __getattribute__(self, name):
            return ''

    Style = Empty()
    Fore = Empty()

colorer = ColorDummy()

def info(message):
    print colorer.Style.BRIGHT + message.encode('utf-8')

def green(message):
    print colorer.Style.BRIGHT + colorer.Fore.GREEN + message.encode('utf-8')

def warn(message):
    print colorer.Style.BRIGHT + colorer.Fore.YELLOW + message.encode('utf-8')

def err(message):
    print colorer.Style.BRIGHT + colorer.Fore.RED + message.encode('utf-8')

def separator():
    separatorLine = '------------------------------------------------'
    info(separatorLine)

def init_color():
    from colorama import Fore, Back, Style, init
    init(autoreset=True) # use Colorama to make Termcolor work on Windows too
    global colorer
    import colorama as colorer
