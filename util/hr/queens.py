import argparse
import time

results = 0

def can_put(board, col, row):
    for i in xrange(col):
        j = board[i]
        if (j == row) or (abs(col - i) == abs(row - j)):
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


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--num', type=int, required=True)
    args = parser.parse_args()

    start = time.clock()
    calculate(args.num)
    spent = time.clock() - start
    print results
    print 'Seconds: ' + str(spent)