import argparse
import time

maharajah = False
results = 0

def can_put(board, col, row):
    for i in xrange(col):
        j = board[i]
        if (j == row) or (abs(col - i) == abs(row - j)):
            return False
    if maharajah:
        if col > 0 and (abs(board[col - 1] - row) == 2):
            return False
        if col > 1 and abs(board[col - 2] - row) == 1:
            return False
    return True

def put_queen(board, col, size):
    log = col + size
    for i in xrange(size):
        valid = can_put(board, col, i)
        if valid:
            board[col] = i
            if (col == size - 1):
                global results
                results += 1
            else:
                put_queen(board, col + 1, size)

def calculate(size):
    board = []
    for i in xrange(size):
        board.append(0)
        
    put_queen(board, 0, size)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--num', type=int, required=True)
    parser.add_argument('-m', '--maharajah', action='store_true', help="Maharajah mode")
    args = parser.parse_args()

    global maharajah
    maharajah = args.maharajah
    
    start = time.clock()
    calculate(args.num)
    spent = time.clock() - start
    print results
    print 'Seconds: ' + str(spent)
    
if __name__ == '__main__':
    main()