#include "movepick.h"
#include <algorithm>

namespace {
// clang-format off
    /**
     * The following tablebase is generated based on ~400k lichess games. It shows
     * relative frequency one square is reached by one type of piece. This hopes to
     * statistically improve move ordering.
     *
     * The criteria for opening is: 1) first 12 moves; 2) both queens on board.
     *
     * The criteria for middle game is: 1) 13~40 moves; 2) at least 7 non-pawn pieces
     * on board.
     *
     * There is currently no endgame tablebase, as in endgames calculation is more
     * important than improving positions.
     */
    constexpr uint16_t OPENING_MOVE_FREQ[14][64] = {
        // P
        {
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
             244,  135,  389,  259,  311,   96,  148,  286,
              88,  126,  401, 1000,  884,  218,   70,   53,
              10,   28,   99,  381,  308,   37,   15,   11,
               1,    5,   18,   26,   16,   16,    4,    1,
               0,    1,    1,    1,    1,    1,    1,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        // N
        {
               0,    4,    1,    2,    5,    7,    2,    0,
               2,    0,    5,  224,  118,    1,    1,   17,
              21,   25,  699,    4,    6, 1000,   32,   14,
              18,    1,   20,  175,   85,   13,    6,   24,
               1,   35,    6,   80,  157,   14,   59,    3,
               0,    4,   47,   11,    8,   17,   10,    0,
               0,    0,    4,    9,    7,    9,    1,    1,
               1,    0,    1,    1,    0,    1,    0,    2,
        },
        // B
        {
               3,   10,    8,    1,    3,   10,    2,    2,
              66,  317,   82,  376,  721,   14,  343,   12,
              39,  252,   60,  961,  647,   94,   98,   11,
             101,   17, 1000,   59,   68,  413,   15,  147,
               2,  569,   21,   87,   46,   26,  721,   12,
              17,    6,  243,   40,   51,  167,   21,   61,
               2,    8,    5,   65,   73,   54,   26,   10,
               5,    4,    2,    5,    0,    4,    5,    2,
        },
        // R
        {
               5,  202,  301,  177, 1000,   36,   49,    3,
              12,    1,    1,    4,    6,   15,    1,    8,
               5,    3,    4,    1,    8,   34,    5,   15,
               5,    1,    1,    3,   15,    9,    1,    3,
               2,    1,    1,    1,   16,    3,    1,    3,
               1,    0,    0,    1,    3,    1,    0,    1,
               1,    1,    0,    1,    1,    1,    2,    1,
              16,    0,    0,    1,    1,    1,    0,    3,
        },
        // Q
        {
              12,   13,   80,  143,  112,    8,    1,    1,
               2,    5,  694,  840, 1000,   26,    4,    2,
              11,  474,   90,  494,  132,  730,  119,   26,
             288,   35,  107,  469,   96,   59,  139,   39,
              11,   84,   31,  152,   75,   21,   34,  274,
              19,   20,   35,   42,   22,   36,   14,   23,
              14,   72,   12,   21,   34,   31,   27,    9,
              15,    2,    5,  134,    2,    2,    2,   15,
        },
        // K
        {
               0,    6,   78,    4,    2,   12, 1000,   29,
               0,    0,    1,    5,    8,   11,    5,   10,
               0,    0,    0,    1,    1,    0,    1,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        // p
        {
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    5,    1,    0,    0,    1,    1,    0,
               2,    5,   30,   11,    6,   12,    3,    2,
              14,   45,  134,  438,  145,   51,   16,    8,
              97,  225,  596, 1000,  716,  146,   72,   50,
             397,  208,  506,  572,  766,  106,  314,  309,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        // n
        {
               1,    0,    0,    0,    0,    1,    0,    1,
               0,    0,    4,    9,    5,    4,    1,    1,
               0,   11,   32,   12,   13,   20,    9,    0,
               1,   31,   16,  107,  145,   11,   54,    3,
              45,    1,   29,  140,  134,   34,    6,   35,
              25,   39,  760,    6,    6, 1000,   47,   28,
               3,    1,    6,  341,  186,    3,    2,   16,
               0,    8,    1,    2,   14,    8,    6,    1,
        },
        // b
        {
               3,    5,    2,    3,    1,    4,    1,    1,
               2,    7,    4,   42,   41,   16,   11,    5,
              10,    7,  167,   62,   25,  173,   12,   24,
               2,  363,   15,   48,   44,   16,  535,   21,
              32,   18,  404,   35,   57,  317,   27,  133,
              60,   86,   64,  460,  308,  116,  110,   16,
              17,  415,   31,  385, 1000,    5,  521,   12,
               4,    6,    7,    3,    4,   13,    1,    3,
        },
        // r
        {
              25,    1,    0,    2,    6,    1,    0,    4,
               1,    5,    1,    2,    2,    1,    3,    2,
               1,    0,    1,    2,    2,    2,    0,    1,
               2,    2,    1,    4,    8,    3,    2,    9,
              14,    2,    2,    4,   17,   11,    2,    6,
              14,    8,    5,    3,    7,   20,    5,   12,
              25,    4,    5,    5,    8,   46,    6,   16,
              10,  405,  498,  176, 1000,   59,  113,    7,
        },
        // q
        {
              15,    2,    6,  118,    4,    2,    1,   12,
              15,   77,   12,   30,   50,   18,   27,   11,
              18,   22,   43,   22,   21,   43,   14,   24,
              15,   96,   23,  182,   96,   30,   36,  214,
             544,   40,  116,  574,  150,   56,  108,   51,
              19,  754,  103,  335,  113,  571,  101,   17,
               6,   16, 1000,  632,  871,   23,    6,    2,
              13,   26,  113,  250,   95,   10,    2,    2,
        },
        // k
        {
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    1,    2,    1,    1,    0,
               0,    0,    1,   10,   13,   30,   12,   11,
               0,    5,   66,    8,    7,   18, 1000,   19,
        },
        // captures by white
        {
               7,    5,    3,    0,    1,    5,    1,    1,
               2,   12,    5,   66,   50,   17,    7,    4,
              10,   40,  294,  104,   51,  242,   27,    9,
               7,   58,  215,  986,  316,   85,   46,   11,
              14,   75,  217, 1000,  595,   94,   44,   16,
              20,   34,  438,  120,  105,  264,   58,   26,
               9,   37,   14,  103,  107,   86,   43,   17,
              21,    4,    5,   55,    2,    8,    5,   14,
        },
        // captures by black
        {
              16,    6,    4,   39,    3,    7,    2,    7,
               7,   39,   14,   73,   68,   26,   21,   10,
              13,   47,  308,   99,   61,  252,   34,   16,
              10,   65,  236, 1000,  404,  112,   39,   18,
               8,   58,  158,  785,  464,   77,   54,   11,
              14,   23,  334,  103,   83,  206,   41,   17,
               2,    5,    4,   79,   76,   49,   20,    7,
               8,    3,    3,    0,    1,    6,    3,    2,
        }
    };

    constexpr uint16_t MIDGAME_MOVE_FREQ[14][64] = {
        // P
        {
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
             564,  701,  605,  123,  181,  737,  739,  763,
             635,  902,  713,  584,  529,  975,  806,  612,
             239,  456,  497,  799, 1000,  604,  402,  282,
              69,  121,  170,  232,  236,  289,  209,   85,
              16,   24,   29,   39,   39,   47,   35,   19,
               3,    3,    4,    5,    4,    6,    2,    2,
        },
        // N
        {
               6,   67,   52,   77,   99,  133,   46,   10,
              35,   37,   95,  754,  504,   83,   46,  165,
             121,  264,  535,  183,  225,  904,  382,   88,
             220,   74,  380,  756,  983,  343,  242,  279,
              68,  346,  354,  665, 1000,  406,  527,  139,
              29,  124,  274,  343,  314,  334,  179,   73,
              32,   63,  123,  187,  193,  155,   47,   33,
              41,   14,   48,   39,   35,   69,    9,   21,
        },
        // B
        {
              38,  130,  222,  130,  125,  179,   23,   14,
             100,  358,  322,  691,  527,  200,  191,   66,
             245,  335,  393,  704, 1000,  571,  382,  132,
             108,  226,  535,  532,  652,  855,  271,  227,
              91,  340,  341,  514,  505,  315,  720,  145,
             153,  139,  303,  316,  268,  543,  220,  396,
              64,  155,  101,  131,  271,  149,  224,  126,
              57,   35,   46,   61,   36,  124,   18,   21,
        },
        // R
        {
             156,  447,  717,  962, 1000,  435,  259,  111,
              72,   68,  120,  172,  181,  207,   65,   37,
              47,   56,   84,  125,  168,  199,  114,   74,
              39,   36,   57,   95,  116,  106,   57,   46,
              39,   37,   65,   90,  113,   74,   42,   39,
              47,   44,   75,   95,   89,   78,   38,   35,
              73,   89,  110,  114,  112,   88,   54,   37,
              57,   39,   83,  123,  116,   87,   19,   32,
        },
        // Q
        {
              63,  109,  200,  274,  256,  150,   34,   20,
              64,  186,  737,  837, 1000,  346,  158,   58,
             105,  512,  394,  816,  600,  878,  507,  204,
             257,  193,  349,  440,  478,  396,  592,  279,
             128,  218,  199,  383,  309,  278,  341,  463,
             149,  134,  231,  221,  267,  236,  232,  258,
             141,  218,  181,  168,  157,  204,  155,  150,
              94,   52,   69,  133,   83,   56,   36,   78,
        },
        // K
        {
              43,  232,  352,  134,  115,  405, 1000,  805,
              35,   78,  116,  215,  273,  394,  562,  495,
              11,   30,   38,   62,   83,  109,  131,   73,
               4,    7,    8,   11,   14,   19,   28,   16,
               2,    2,    2,    2,    3,    4,    6,    5,
               0,    1,    1,    1,    1,    1,    1,    1,
               0,    0,    0,    0,    1,    1,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        // p
        {
               3,    4,    4,    4,    3,    4,    2,    1,
              16,   25,   26,   27,   23,   24,   23,   13,
              69,  133,  174,  149,  131,  149,  117,   60,
             238,  473,  502,  668,  680,  438,  319,  190,
             637,  880,  706,  774,  830, 1000,  700,  542,
             510,  529,  473,  163,  283,  758,  831,  733,
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
        },
        // n
        {
              29,   14,   37,   32,   34,   67,    8,   14,
              27,   65,  106,  149,  154,  125,   39,   24,
              29,  125,  245,  327,  302,  260,  179,   71,
              74,  286,  416,  552,  904,  452,  420,  123,
             201,   74,  428,  840, 1000,  396,  194,  330,
             117,  327,  546,  182,  210,  930,  358,  105,
              37,   44,  109,  794,  483,   83,   68,  123,
               9,   76,   62,   69,  119,  143,   55,   12,
        },
        // b
        {
              69,   37,   56,   61,   49,  116,   16,   30,
              62,  199,  113,  131,  202,  160,  172,   99,
             153,  120,  361,  309,  294,  564,  201,  288,
             118,  338,  322,  605,  577,  344,  596,  247,
              88,  329,  620,  636,  766,  795,  525,  192,
             384,  224,  577,  759,  939, 1000,  360,  242,
              64,  635,  191,  860,  722,  156,  346,   59,
              68,   64,  310,  169,  170,  324,   16,   35,
        },
        // r
        {
              62,   46,   94,  128,  120,   93,   20,   32,
              79,  101,  121,  110,  103,   89,   53,   39,
              48,   47,   90,   84,   80,   82,   40,   38,
              41,   48,   92,   93,  102,   83,   43,   43,
              40,   40,   67,   99,  109,  111,   53,   49,
              51,   54,   83,  106,  135,  177,   93,   59,
              81,   78,  138,  181,  176,  228,   75,   47,
             176,  488,  839,  989, 1000,  445,  281,  129,
        },
        // q
        {
              97,   61,   85,  129,   92,   57,   40,   73,
             162,  272,  201,  179,  141,  190,  148,  127,
             151,  132,  255,  202,  261,  225,  234,  222,
             146,  251,  234,  438,  305,  332,  305,  440,
             354,  218,  418,  438,  505,  370,  568,  255,
             131,  700,  469,  707,  474,  815,  412,  176,
              97,  263,  936,  849, 1000,  313,  177,   50,
              89,  147,  260,  370,  293,  171,   40,   25,
        },
        // k
        {
               0,    0,    0,    0,    0,    0,    0,    0,
               0,    0,    0,    0,    0,    0,    0,    0,
               1,    0,    1,    1,    1,    1,    1,    1,
               3,    3,    2,    2,    3,    4,    7,    5,
               6,    8,    9,    9,   12,   18,   24,   18,
              11,   31,   43,   59,   70,  109,  114,   75,
              28,   73,  127,  231,  280,  383,  616,  418,
              38,  183,  311,  147,  132,  393, 1000,  664,
        },
        // wcapture
        {
              66,   44,  105,  159,  118,  148,   20,   26,
              30,   75,   81,  168,  170,  130,  101,   41,
              62,  165,  326,  272,  286,  465,  208,   83,
              82,  274,  412,  730,  826,  403,  312,   98,
              119,  347,  465,  931, 1000,  513,  378,  157,
              166,  192,  448,  406,  464,  646,  360,  218,
              144,  258,  175,  254,  315,  298,  260,  139,
              127,   62,  144,  220,  163,  219,   35,   57,
        },
        // bcapture
        {
             113,   63,  139,  197,  158,  197,   31,   47,
             147,  286,  187,  202,  200,  244,  195,  102,
             136,  200,  448,  327,  362,  529,  274,  191,
             123,  300,  440,  820,  838,  462,  324,  153,
              73,  288,  397,  794, 1000,  446,  364,  100,
              70,  143,  305,  305,  323,  564,  262,  105,
              29,   81,   79,  192,  234,  150,  153,   53,
              69,   44,  109,  166,  121,  165,   21,   29,
        },
    };

    /**
     * MVV-LVA table, indexed by [aggressor][victim]. Prefer simplifying captures
     * (i.e. QxQ comes before RxR). Prefer trading knights over bishops.
     */
    constexpr int16_t MVV_LVA_TABLE[7][7] = {
        //         P     N     B     R     Q      K none
        /* P */{   0,  200,  250,  450,  900,     0,     0},
        /* N */{-200,   10,   50,  250,  700,     0,     0},
        /* B */{-250,  -50,    5,  200,  650,     0,     0},
        /* R */{-450, -250, -200,   15,  450,     0,     0},
        /* Q */{-900, -700, -650, -450,   20,     0,     0},
        /* K */{   0,    0,    0,    0,    0,     0,     0},
        /*   */{   0,    0,    0,    0,    0,     0,     0},
    };

    constexpr int16_t PROMOTION_BONUS = 500;
    constexpr int16_t HANGING_PENALTY = 400;
    constexpr int16_t QUEEN_HANGING_PENALTY = 800;
    constexpr int16_t ESCAPING_BONUS = 200;
    constexpr int16_t CHECK_BONUS = 50;
// clang-format on
} // namespace

MovePicker::MovePicker(Position &pos) : pos(pos) {
}

void MovePicker::init(KillerTbl *killerTable = nullptr,
                      HistTbl *histTable = nullptr) {
    Movelist moves = pos.legalMoves();
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move move = moves[i];
        const bool isWhite = pos.sideToMove() == WHITE;

        int16_t s = 0;
        if (killerTable && killerTable->has(move)) {
            s = 30000; // Killer moves has to be searched first
        } else {
            s = score(move);
            if (histTable) {
                PieceType pt = pos.at(move.from()).type();
                s += histTable->get(move, pt, isWhite);
            }
        }

        q.emplace_back(move.move(), s);
    }

    std::sort(q.begin(), q.end(), std::less<>()); // ascending order
}

void MovePicker::initQuiet(HistTbl *historyTable = nullptr) {
    Movelist moves = pos.generateCaptureMoves();
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move move = moves[i];
        const bool isWhite = pos.sideToMove() == WHITE;

        int16_t s = score(move);
        if (historyTable) {
            PieceType pt = pos.at(move.from()).type();
            s += historyTable->get(move, pt, isWhite);
        }

        q.emplace_back(move.move(), s);
    }

    std::sort(q.begin(), q.end(), std::less<>()); // ascending order
}

/**
 * Scores a move without any history context.
 */
int16_t MovePicker::score(Move move) {
    int16_t s = 0;
    bool likelyBlunder = false;
    const Color stm = pos.sideToMove();

    // Apply MVV/LVA heuristic
    PieceType aggressor = pos.at(move.from()).type();
    PieceType victim = pos.at(move.to()).type();
    const int16_t mvvlva = MVV_LVA_TABLE[(int)aggressor][(int)victim];
    const bool isCapture = pos.isCapture(move);
    if (mvvlva < 0) {
        likelyBlunder = true; // losing capture
    }
    s += mvvlva;

    // Give bonus to queen promotion or knight promotion with check
    const bool givesCheck = pos.isCheckMove(move);
    if (move.typeOf() == Move::PROMOTION &&
        ((move.promotionType() == TYPE_QUEEN) ||
         (givesCheck && move.promotionType() == TYPE_KNIGHT))) {
        s += PROMOTION_BONUS;
    }

    // Penalty for moving to a square controlled by opponent pawns
    if (aggressor != TYPE_PAWN &&
        (attacks::pawn(~stm, move.to()) & pos.pieces(TYPE_PAWN, ~stm))) {
        s -= HANGING_PENALTY;
        likelyBlunder = true;
    }
    // Penalty for queens to move to a square attacked by opponent
    if (aggressor == TYPE_QUEEN) {
        Bitboard enemy = pos.pieces(TYPE_ROOK, ~stm) |
                         pos.pieces(TYPE_BISHOP, ~stm) |
                         pos.pieces(TYPE_KNIGHT, ~stm);
        Bitboard range = attacks::queen(move.to(), pos.occ());
        if (enemy & range) {
            s -= HANGING_PENALTY;
            likelyBlunder = true;
        }
    }

    // Bonus for escaping from a square controlled by opponent pawns
    if (aggressor != TYPE_PAWN &&
        ((attacks::pawn(~stm, move.from()) & pos.pieces(TYPE_PAWN, ~stm)))) {
        s += ESCAPING_BONUS;
    }

    // Bonus for checking the king if this move isn't an obvious blunder
    if (!likelyBlunder && givesCheck) {
        s += CHECK_BONUS;
    }

    // Statistical bonus, even more if this move is not an obvious blunder
    const int divider = likelyBlunder ? 15 : 50;
    const bool queensOnBoard = pos.countPieces(TYPE_QUEEN) == 2;
    if (pos.fullMoveNumber() <= 12 && queensOnBoard) { // opening
        s += (int)(OPENING_MOVE_FREQ[(int)aggressor][move.to().index()] /
                   divider);
        if (isCapture) {
            s += (int)(OPENING_MOVE_FREQ[stm == WHITE ? 12 : 13]
                                        [move.to().index()] /
                       divider);
        }
    } else if (pos.fullMoveNumber() <= 40) {
        const int nonPawnPieces = (pos.occ() ^ pos.pieces(TYPE_PAWN)).count();
        if (nonPawnPieces <= 7) {
            return;
        }
        s += (int)(MIDGAME_MOVE_FREQ[(int)aggressor][move.to().index()] /
                   divider);
        if (isCapture) {
            s += (int)(MIDGAME_MOVE_FREQ[stm == WHITE ? 12 : 13]
                                        [move.to().index()] /
                       divider);
        }
    }

    return s;
}

inline Move MovePicker::pick() {
    if (q.size() == 0) {
        return Move::NO_MOVE;
    } else {
        Move m = q.back().move();
        q.pop_back();
        return m;
    }
}