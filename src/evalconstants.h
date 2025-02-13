#include "eval.h"

using S = Score;

// clang-format off
/**
 * Base values for all pieces
 */
constexpr S PIECE_VALUE[] = {
    S( 89, 103),S(286,328),S(312,356),S(538,590),S(1043,1100),S(  0,  0)
};

/**
 * Piece square tables. Generally, we hope these tables help pieces
 * develop to a better square.
 * 
 * Values based on Stockfish 6's PSQT.
 * Reference: https://github.com/official-stockfish/Stockfish/blob/sf_6/src/psqtab.h
 */
constexpr S PIECE_SQUARE_TABLES[6][64] = {
    // Pawns are encouraged to control the center
    {
        S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),
        S(-20,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(-20,  0),
        S(-15,  0),S(  0,  0),S( 10,  0),S( 20,  0),S( 20,  0),S( 10,  0),S(  0,  0),S(-15,  0),
        S(-20,  0),S(  0,  0),S( 20,  0),S( 40,  0),S( 40,  0),S( 20,  0),S(  0,  0),S(-20,  0),
        S(-20,  0),S(  0,  0),S( 10,  0),S( 20,  0),S( 20,  0),S( 10,  0),S(  0,  0),S(-20,  0),
        S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),
        S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),
        S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),S(  0,  0),
    },
    // Knights are most useful if pushed towards the center
    {
        S(-144,-98),S(-109,-83),S(-85,-51),S(-73,-16),S(-73,-16),S(-85,-51),S(-109,-83),S(-144,-98),
        S( -88,-68),S( -43,-53),S(-19,-21),S( -7, 14),S( -7, 14),S(-19,-21),S( -43,-53),S( -88,-68),
        S( -69,-53),S( -24,-38),S(  0, -6),S( 12, 29),S( 12, 29),S(  0, -6),S( -24,-38),S( -69,-53),
        S( -28,-42),S(  17,-27),S( 41,  5),S( 53, 40),S( 53, 40),S( 41,  5),S(  17,-27),S( -28,-42),
        S( -30,-42),S(  15,-27),S( 39,  5),S( 51, 40),S( 51, 40),S( 39,  5),S(  15,-27),S( -30,-42),
        S( -10,-53),S(  35,-38),S( 59, -6),S( 71, 29),S( 71, 29),S( 59, -6),S(  35,-38),S( -10,-53),
        S( -64,-68),S( -19,-53),S(  5,-21),S( 17, 14),S( 17, 14),S(  5,-21),S( -19,-53),S( -64,-68),
        S(-200,-98),S( -65,-83),S(-41,-51),S(-29,-16),S(-29,-16),S(-41,-51),S( -65,-83),S(-200,-98),
    },
    // Bishops are most useful if they control diagnoals
    {
        S(-54,-65),S(-27,-42),S(-34,-44),S(-43,-26),S(-43,-26),S(-34,-44),S(-27,-42),S(-54,-65),
        S(-29,-43),S(  8,-20),S(  1,-22),S( -8, -4),S( -8, -4),S(  1,-22),S(  8,-20),S(-29,-43),
        S(-20,-33),S( 17,-10),S( 10,-12),S(  1,  6),S(  1,  6),S( 10,-12),S( 17,-10),S(-20,-33),
        S(-19,-35),S( 18,-12),S( 11,-14),S(  2,  4),S(  2,  4),S( 11,-14),S( 18,-12),S(-19,-35),
        S(-22,-35),S( 15,-12),S(  8,-14),S( -1,  4),S( -1,  4),S(  8,-14),S( 15,-12),S(-22,-35),
        S(-28,-33),S(  9,-10),S(  2,-12),S( -7,  6),S( -7,  6),S(  2,-12),S(  9,-10),S(-28,-33),
        S(-32,-43),S(  5,-20),S( -2,-22),S(-11, -4),S(-11, -4),S( -2,-22),S(  5,-20),S(-32,-43),
        S(-49,-65),S(-22,-42),S(-29,-44),S(-38,-26),S(-38,-26),S(-29,-44),S(-22,-42),S(-49,-65)
    },
    // Rooks generally is not so sensitive to specific squares... But they can
    // infiltrate on the 7th rank!
    {
        S(-22,  3),S(-17,  3),S(-12,  3),S( -8,  3),S( -8,  3),S(-12,  3),S(-17,  3),S(-22,  3),
        S(-22,  3),S( -7,  3),S( -2,  3),S(  2,  3),S(  2,  3),S( -2,  3),S( -7,  3),S(-22,  3),
        S(-22,  3),S( -7,  3),S( -2,  3),S(  2,  3),S(  2,  3),S( -2,  3),S( -7,  3),S(-22,  3),
        S(-22,  3),S( -7,  3),S( -2,  3),S(  2,  3),S(  2,  3),S( -2,  3),S( -7,  3),S(-22,  3),
        S(-22,  3),S( -7,  3),S( -2,  3),S(  2,  3),S(  2,  3),S( -2,  3),S( -7,  3),S(-22,  3),
        S(-22,  3),S( -7,  3),S( -2,  3),S(  2,  3),S(  2,  3),S( -2,  3),S( -7,  3),S(-22,  3),
        S( -6,  3),S(  9,  3),S( 14,  3),S( 18,  3),S( 18,  3),S( 14,  3),S(  9,  3),S( -6,  3),
        S(-22,  3),S(-17,  3),S(-12,  3),S( -8,  3),S( -8,  3),S(-12,  3),S(-17,  3),S(-22,  3)
    },
    // Queens are even less sensitive to specific squares
    {
        S(-2,-80),S(-2,-54),S(-2,-42),S(-2,-30),S(-2,-30),S(-2,-42),S(-2,-54),S(-2,-80),
        S(-2,-54),S( 8,-30),S( 8,-18),S( 8, -6),S( 8, -6),S( 8,-18),S( 8,-30),S(-2,-54),
        S(-2,-42),S( 8,-18),S( 8, -6),S( 8,  6),S( 8,  6),S( 8, -6),S( 8,-18),S(-2,-42),
        S(-2,-30),S( 8, -6),S( 8,  6),S( 8, 18),S( 8, 18),S( 8,  6),S( 8, -6),S(-2,-30),
        S(-2,-30),S( 8, -6),S( 8,  6),S( 8, 18),S( 8, 18),S( 8,  6),S( 8, -6),S(-2,-30),
        S(-2,-42),S( 8,-18),S( 8, -6),S( 8,  6),S( 8,  6),S( 8, -6),S( 8,-18),S(-2,-42),
        S(-2,-54),S( 8,-30),S( 8,-18),S( 8, -6),S( 8, -6),S( 8,-18),S( 8,-30),S(-2,-54),
        S(-2,-80),S(-2,-54),S(-2,-42),S(-2,-30),S(-2,-30),S(-2,-42),S(-2,-54),S(-2,-80)
    },
    // Kings. Important for safety. Encouraged to stay at the corner in middle
    // game and move towards the center in the endgame.
    {
        S(298, 27),S(332, 81),S(273,108),S(225,116),S(225,116),S(273,108),S(332, 81),S(298, 27),
        S(287, 74),S(321,128),S(262,155),S(214,163),S(214,163),S(262,155),S(321,128),S(287, 74),
        S(224,111),S(258,165),S(199,192),S(151,200),S(151,200),S(199,192),S(258,165),S(224,111),
        S(196,135),S(230,189),S(171,216),S(123,224),S(123,224),S(171,216),S(230,189),S(196,135),
        S(173,135),S(207,189),S(148,216),S(100,224),S(100,224),S(148,216),S(207,189),S(173,135),
        S(146,111),S(180,165),S(121,192),S( 73,200),S( 73,200),S(121,192),S(180,165),S(146,111),
        S(119, 74),S(153,128),S( 94,155),S( 46,163),S( 46,163),S( 94,155),S(153,128),S(119, 74),
        S( 98, 27),S(132, 81),S( 73,108),S( 25,116),S( 25,116),S( 73,108),S(132, 81),S( 98, 27)
    }
};

constexpr S MOBILITY_BONUS[6][32] = {
    // Unused
    {},
    // Knights
    {
        S(-31,-40),S(-26,-28),S( -6,-15),S( -2, -8),S(  1,  2),S(  6,  5),S( 11,  8),S( 14, 10),
        S( 16, 12)
    },
    // Bishops
    {
        S(-24,-29),S(-10,-11),S(  8, -1),S( 13,  6),S( 19, 12),S( 25, 21),S( 27, 27),S( 31, 28),
        S( 31, 32),S( 34, 36),S( 40, 39),S( 40, 43),S( 45, 44),S( 49, 48)
    },
    // Rooks
    {
        S(-30,-39),S(-10, -8),S(  1, 11),S(  1, 19),S(  1, 35),S(  5, 49),S( 11, 51),S( 15, 60),
        S( 20, 67),S( 20, 69),S( 20, 79),S( 24, 82),S( 28, 84),S( 28, 84),S( 31, 86)
    },
    // Queens
    {
        S(-15,-24),S( -6,-15),S( -4, -3),S( -4,  9),S( 10, 20),S( 11, 27),S( 11, 29),S( 17, 37),
        S( 19, 39),S( 26, 48),S( 32, 48),S( 32, 50),S( 32, 60),S( 33, 63),S( 33, 65),S( 33, 66),
        S( 36, 68),S( 36, 70),S( 38, 73),S( 39, 75),S( 46, 75),S( 54, 84),S( 54, 84),S( 54, 85),
        S( 55, 91),S( 57, 91),S( 57, 96),S( 58,109)
    },
    // Unused
    {}
};

constexpr S SPACE_BONUS = S(  2,  0);
constexpr S ISOLATED_PAWN_PENALTY = S(-2, -8);
constexpr S DOUBLED_PAWN_PENALTY = S(-5, -28);
constexpr S BISHOP_PAIR_BONUS = S(20, 30);
constexpr S SUPPORTED_PAWN_BONUS = S(10, 6);
constexpr S PASSED_PAWN_BONUS[] = {
    S(  0,  0),S(138,130),S( 84, 88),S( 31, 36),S(  8, 20),S(  5, 16),S(  0,  8),S(  0,  0)
};
constexpr S UNBLOCKED_PASSED_PAWN_BONUS[] = {
    S(  0,  0),S(200,200),S(150,150),S(100,100),S( 20, 20),S(  0,  0),S(  0,  0),S(  0,  0)
};


constexpr int KING_ATTACKER_WEIGHT[] = {
    0, 40, 21, 22, 5, 0
};
constexpr Value WEAK_KING_SQUARE_PENALTY = Value(-8);

constexpr Value TEMPO_BONUS = Value(13);


/**
 * Bitboard masks to test adjacent/neighbor files of a square. Used to 
 * test isolated pawns.
 */
constexpr Bitboard PAWN_NEIGHBORING_FILES[8] = {
    Bitboard(0x4040404040404040),
    Bitboard(0xA0A0A0A0A0A0A0A0),
    Bitboard(0x5050505050505050),
    Bitboard(0x2828282828282828),
    Bitboard(0x1414141414141414),
    Bitboard(0x0A0A0A0A0A0A0A0A),
    Bitboard(0x0505050505050505),
    Bitboard(0x0202020202020202)
};

/**
 * King rings are 3x3 arears around the king. 
 */
constexpr Bitboard KING_RING_BB[64] = {
    0x70707, 0x70707, 0xe0e0e, 0x1c1c1c, 0x383838, 0x707070, 
    0xe0e0e0, 0xe0e0e0, 0x70707, 0x70707, 0xe0e0e, 0x1c1c1c, 
    0x383838, 0x707070, 0xe0e0e0, 0xe0e0e0, 0x7070700, 0x7070700, 
    0xe0e0e00, 0x1c1c1c00, 0x38383800, 0x70707000, 0xe0e0e000, 0xe0e0e000, 
    0x707070000, 0x707070000, 0xe0e0e0000, 0x1c1c1c0000, 0x3838380000, 0x7070700000, 
    0xe0e0e00000, 0xe0e0e00000, 0x70707000000, 0x70707000000, 0xe0e0e000000, 0x1c1c1c000000, 
    0x383838000000, 0x707070000000, 0xe0e0e0000000, 0xe0e0e0000000, 0x7070700000000, 0x7070700000000, 
    0xe0e0e00000000, 0x1c1c1c00000000, 0x38383800000000, 0x70707000000000, 0xe0e0e000000000, 0xe0e0e000000000, 
    0x707070000000000, 0x707070000000000, 0xe0e0e0000000000, 0x1c1c1c0000000000, 0x3838380000000000, 0x7070700000000000, 
    0xe0e0e00000000000, 0xe0e0e00000000000, 0x707070000000000, 0x707070000000000, 0xe0e0e0000000000, 0x1c1c1c0000000000, 
    0x3838380000000000, 0x7070700000000000, 0xe0e0e00000000000, 0xe0e0e00000000000
};

constexpr Bitboard PASSED_PAWN_DETECT_MASK[2][64] = {
    // White
    {
        0x303030303030300, 0x707070707070700, 0xe0e0e0e0e0e0e00, 0x1c1c1c1c1c1c1c00, 
        0x3838383838383800, 0x7070707070707000, 0xe0e0e0e0e0e0e000, 0xc0c0c0c0c0c0c000, 
        0x303030303030000, 0x707070707070000, 0xe0e0e0e0e0e0000, 0x1c1c1c1c1c1c0000, 
        0x3838383838380000, 0x7070707070700000, 0xe0e0e0e0e0e00000, 0xc0c0c0c0c0c00000, 
        0x303030303000000, 0x707070707000000, 0xe0e0e0e0e000000, 0x1c1c1c1c1c000000, 
        0x3838383838000000, 0x7070707070000000, 0xe0e0e0e0e0000000, 0xc0c0c0c0c0000000, 
        0x303030300000000, 0x707070700000000, 0xe0e0e0e00000000, 0x1c1c1c1c00000000, 
        0x3838383800000000, 0x7070707000000000, 0xe0e0e0e000000000, 0xc0c0c0c000000000, 
        0x303030000000000, 0x707070000000000, 0xe0e0e0000000000, 0x1c1c1c0000000000, 
        0x3838380000000000, 0x7070700000000000, 0xe0e0e00000000000, 0xc0c0c00000000000, 
        0x303000000000000, 0x707000000000000, 0xe0e000000000000, 0x1c1c000000000000, 
        0x3838000000000000, 0x7070000000000000, 0xe0e0000000000000, 0xc0c0000000000000, 
        0x300000000000000, 0x700000000000000, 0xe00000000000000, 0x1c00000000000000, 
        0x3800000000000000, 0x7000000000000000, 0xe000000000000000, 0xc000000000000000, 
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    },
    // Black
    {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x7, 0xe, 0x1c, 0x38, 0x70, 
        0xe0, 0xc0, 0x303, 0x707, 0xe0e, 0x1c1c, 0x3838, 0x7070, 0xe0e0, 0xc0c0, 
        0x30303, 0x70707, 0xe0e0e, 0x1c1c1c, 0x383838, 0x707070, 0xe0e0e0, 0xc0c0c0, 
        0x3030303, 0x7070707, 0xe0e0e0e, 0x1c1c1c1c, 0x38383838, 0x70707070, 
        0xe0e0e0e0, 0xc0c0c0c0, 0x303030303, 0x707070707, 0xe0e0e0e0e, 0x1c1c1c1c1c, 
        0x3838383838, 0x7070707070, 0xe0e0e0e0e0, 0xc0c0c0c0c0, 0x30303030303, 
        0x70707070707, 0xe0e0e0e0e0e, 0x1c1c1c1c1c1c, 0x383838383838, 0x707070707070, 
        0xe0e0e0e0e0e0, 0xc0c0c0c0c0c0, 0x3030303030303, 0x7070707070707, 0xe0e0e0e0e0e0e, 
        0x1c1c1c1c1c1c1c, 0x38383838383838, 0x70707070707070, 0xe0e0e0e0e0e0e0, 0xc0c0c0c0c0c0c0 
    }
};