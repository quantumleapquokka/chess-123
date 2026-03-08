#include "Chess.h"
#include "Bitboard.h"
#include <limits>
#include <cmath>
#include <array>
#include <cstdint>
#include <vector>

namespace {
    inline int sqIndex(int x, int y) {
        return (7 - y) * 8 + x;
    }

    inline bool inBounds(int x, int y) {
        return x >= 0 && x < 8 && y >= 0 && y < 8;
    }

    inline bool findHolderXY(Grid* grid, BitHolder* h, int& outX, int& outY) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (grid->getSquare(x, y) == h) {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
        
        return false;
    }

    inline void buildOccupancy(Grid* grid, uint64_t& occ0, uint64_t& occ128) {
        occ0 = 0;
        occ128 = 0;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                auto sq = grid->getSquare(x, y);
                Bit* b = sq ? sq->bit() : nullptr;
                if (!b) continue;

                uint64_t mask = 1ULL << sqIndex(x, y);
                if ((b->gameTag() & 128) == 0) occ0 |= mask;
                else                           occ128 |= mask;
            }
        }
    }

    inline std::array<uint64_t, 64>& knightAttacks() {
        static std::array<uint64_t, 64> table{};
        static bool init = false;
        if (!init) {
            init = true;
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    uint64_t bb = 0;
                    const int dx[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
                    const int dy[8] = { 2, 1, -1, -2, -2, -1, 1, 2 };
                    for (int i = 0; i < 8; i++) {
                        int nx = x + dx[i];
                        int ny = y + dy[i];
                        if (inBounds(nx, ny)) bb |= 1ULL << sqIndex(nx, ny);
                    }
                    table[sqIndex(x, y)] = bb;
                }
            }
        }
        return table;
    }

    inline std::array<uint64_t, 64>& kingAttacks() {
        static std::array<uint64_t, 64> table{};
        static bool init = false;
        if (!init) {
            init = true;
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x++) {
                    uint64_t bb = 0;
                    for (int oy = -1; oy <= 1; oy++) {
                        for (int ox = -1; ox <= 1; ox++) {
                            if (ox == 0 && oy == 0) continue;
                            int nx = x + ox;
                            int ny = y + oy;
                            if (inBounds(nx, ny)) bb |= 1ULL << sqIndex(nx, ny);
                        }
                    }
                    table[sqIndex(x, y)] = bb;
                }
            }
        }
        return table;
    }

    inline ChessPiece pieceFromTag(int tag) {
        int idx = tag & 0x7F;
        switch (idx) {
            case 1: return Pawn;
            case 2: return Knight;
            case 6: return King;
            case 3: return Bishop;
            case 4: return Rook;
            case 5: return Queen;
            default: return NoPiece;
        }
    }
}

Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    startGame();

    std::vector<BitMove> moves = generateAllMoves();
    int n = static_cast<int>(moves.size());
    (void)n;
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)
    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });

    size_t pos = fen.find(" ");
    std::string placement = fen;

    if (pos != std::string::npos) {
        placement = fen.substr(0, pos);
    }

    int x = 0;
    int rankIndex = 0;

    for (char c: placement) {
        if(c == '/') {
            rankIndex += 1;
            x = 0;
            continue;
        }

        if (c >= '1' && c <= '8') {
            x = x + (c - '0');
            continue;
        }

        int color = (std::isupper(static_cast<unsigned char>(c)) ? 1 : 0);

        char t = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            ChessPiece piece;
            switch (t) {
            case 'p': piece = Pawn;   break;
            case 'n': piece = Knight; break;
            case 'b': piece = Bishop; break;
            case 'r': piece = Rook;   break;
            case 'q': piece = Queen;  break;
            case 'k': piece = King;   break;
            default:
                continue;
        }

        Bit* bit = PieceForPlayer(color, piece);

        int pieceIndex = 0;
        switch (piece) {
            case Pawn:   pieceIndex = 1; break;
            case Knight: pieceIndex = 2; break;
            case Bishop: pieceIndex = 3; break;
            case Rook:   pieceIndex = 4; break;
            case Queen:  pieceIndex = 5; break;
            case King:   pieceIndex = 6; break;
            default:     pieceIndex = 0; break;
        }

        int tag = (color == 0 ? 0 : 128) + pieceIndex;
        bit->setGameTag(tag);

        ChessSquare* sq = _grid->getSquare(x, rankIndex);
        sq->setBit(bit);                 // attaches to holder
        bit->setParent(sq);              // redundant but safe
        bit->setPosition(sq->getPosition());

        x += 1;
    }
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    if (!canBitMoveFrom(bit, src)) return false;

    int sx, sy, dx, dy;
    if (!findHolderXY(_grid, &src, sx, sy)) return false;
    if (!findHolderXY(_grid, &dst, dx, dy)) return false;

    ChessPiece p = pieceFromTag(bit.gameTag());

    // Occupancy
    uint64_t occ0, occ128;
    buildOccupancy(_grid, occ0, occ128);

    const int colorBit = (bit.gameTag() & 128);
    const uint64_t friendlyOcc = (colorBit == 0) ? occ0 : occ128;
    const uint64_t enemyOcc    = (colorBit == 0) ? occ128 : occ0;


    {
        Bit* dstBit = dst.bit();
        if (dstBit && ((dstBit->gameTag() & 128) == colorBit)) return false;
    }

    // Knight
    if (p == Knight) {
        uint64_t attacks = knightAttacks()[sqIndex(sx, sy)];
        attacks &= ~friendlyOcc;

        uint64_t targetMask = 1ULL << sqIndex(dx, dy);
        return (attacks & targetMask) != 0;
    }

    // King
    if (p == King) {
        uint64_t attacks = kingAttacks()[sqIndex(sx, sy)];
        attacks &= ~friendlyOcc;

        uint64_t targetMask = 1ULL << sqIndex(dx, dy);
        return (attacks & targetMask) != 0;
    }

    // Pawn
    if (p == Pawn) {
        int dir = (colorBit == 128) ? -1 : +1;

        if (dx == sx && dy == sy + dir) {
            return dst.bit() == nullptr;
        }

        const int startRank = (colorBit == 128) ? 6 : 1;
        if (sx == dx && sy == startRank && dy == sy + 2 * dir) {
            int midY = sy + dir;
            auto midSq = _grid->getSquare(sx, midY);
            if (!midSq) return false;
            if (midSq->bit() != nullptr) return false;
            return dst.bit() == nullptr;
        }

        if (dy == sy + dir && (dx == sx - 1 || dx == sx + 1)) {
            Bit* dstBit = dst.bit();
            if (!dstBit) return false;
            return ((dstBit->gameTag() & 128) != colorBit);
        }

        return false;
    }

    // Rook
    if (p == Rook) {
        if (sx != dx && sy != dy) return false;
        if (sx == dx && sy == dy) return false;

        int X = 0;
        int Y = 0;

        if (dx > sx) X = 1;
        if (dx < sx) X = -1;
        if (dy > sy) Y = 1;
        if (dy < sy) Y = -1;

        int wX = sx + X;
        int wY = sy + Y;

        while (wX != dx || wY != dy) {
            auto sq = _grid->getSquare(wX, wY);
            if (!sq) return false;
            
            if (sq->bit() != nullptr) return false;

            wX += X;
            wY += Y;
        }
        return true;
    }

    // Bishop
    if (p == Bishop) {
        int diffX = dx - sx;
        int diffY = dy - sy;

        if (abs(diffX) != abs(diffY)) return false;
        if (diffX == 0) return false;

        int X = (diffX > 0) ? 1 : -1;
        int Y = (diffY > 0) ? 1 : -1;

        int x = sx + X;
        int y = sy + Y;

        while (x != dx || y != dy) {
            auto sq = _grid->getSquare(x, y);
            if (!sq) return false;

            if (sq->bit() != nullptr) return false;

            x += X;
            y += Y;
        }

        return true;
    }

    // Queen
    if (p == Queen) {
        int diffX = dx - sx;
        int diffY = dy - sy;

        if (sx == dx || sy == dy) {
            if (sx == dx && sy == dy) return false;

            int X = 0;
            int Y = 0;

            if (dx > sx) X = 1;
            if (dx < sx) X = -1;
            if (dy > sy) Y = 1;
            if (dy < sy) Y = -1;

            int wX = sx + X;
            int wY = sy + Y;

            while (wX != dx || wY != dy) {
                auto sq = _grid->getSquare(wX, wY);
                if (!sq) return false;
                
                if (sq->bit() != nullptr) return false;

                wX += X;
                wY += Y;
            }
            return true;
        }

        if (abs(diffX) == abs(diffY)) {
            int X = (diffX > 0) ? 1 : -1;
            int Y = (diffY > 0) ? 1 : -1;

            int x = sx + X;
            int y = sy + Y;

            while (x != dx || y != dy) {
                auto sq = _grid->getSquare(x, y);
                if (!sq) return false;
                if (sq->bit() != nullptr) return false;

                x += X;
                y += Y;
            }

            return true;
        }

        return false;
    }

    return false;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
            s += pieceNotation( x, y );
        }
    );
    return s;}

void Chess::setStateString(const std::string &s)
{
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}

std::vector<BitMove> Chess::generateAllMoves()
{
    std::vector<BitMove> moves;

    for (int sy = 0; sy < 8; sy++) {
        for (int sx = 0; sx < 8; sx++) {
            ChessSquare* src = _grid->getSquare(sx, sy);
            if (!src) continue;

            Bit* bit = src->bit();
            if (!bit) continue;

            if (!canBitMoveFrom(*bit, *src)) continue;

            ChessPiece piece = pieceFromTag(bit->gameTag());

            for (int dy = 0; dy < 8; dy++) {
                for (int dx = 0; dx < 8; dx++) {
                    ChessSquare* dst = _grid->getSquare(dx, dy);
                    if (!dst) continue;

                    if (canBitMoveFromTo(*bit, *src, *dst)) {
                        int from = sqIndex(sx, sy);
                        int to   = sqIndex(dx, dy);
                        moves.push_back(BitMove(from, to, piece));
                    }
                }
            }
        }
    }

    return moves;
}

int Chess::moveGenerator(BitMove* out, int maxMoves)
{
    std::vector<BitMove> moves = generateAllMoves();

    int count = static_cast<int>(moves.size());
    if (count > maxMoves) count = maxMoves;

    for (int i = 0; i < count; i++) {
        out[i] = moves[i];
    }

    return count;
}