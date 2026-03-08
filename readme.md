Added BitBoard.h and took out the enum from Chess.h because of the conflicting definition.
Movmeent added for pawns, knights, and king. Pawns can move forward one or two spaces and capture diagonally. Knights move the way they are suppposed to and can capture. King also moves as it's supposed to, one square away and can capture.
Capture remove the other player's piece for the board.
Both black and white can capture.
Move generator for 20 moves to produce moves for current player seen in screenshot.
Board screenshot also included in root.

ADDED: rook, bishop, and queen movement
Rooks go either up/down/accross and Bishops go diagonally on the same color they start as. Queens used the movement of rooks and bishops combined since they move up/down/accross and diagonally. Added generateAllMoves() for current side to play and updated move generation to have legal movement and captures.