#include <cassert>
#include <algorithm>
#include <regex>
#include <sstream>
#include <vector>
#include "san.h"
#include "movegen.h"
#include "uci.h"

// This is all about handling algebraic notation (AN).
// We distinguish between ...
// 1. LAN (long algebraic notation) -> standard in UCI.
// 2. SAN (standard algebraic notation) -> used in PGN files and other displays.

namespace Stockfish::SAN
{
	// Helper: returns square from an AN string.
	static Square get_square_from(const std::string& s)
	{
		return make_square(File(s[0] - 'a'), Rank(s[1] - '1'));
	}

	// Helper: returns a Move from the given parameters.
	using ret = std::pair<std::string, Move>;

	static ret get_move_from(const Position& pos, PieceType pt, Square to, bool promotion, File file, Rank rank)
	{
		MoveList<LEGAL> ml(pos);
		std::vector<ExtMove> list(ml.begin(), ml.end());
			
		std::erase_if(list, [&](const auto& m)
			{
				if (to_sq(m) != to || type_of(pos.piece_on(from_sq(m))) != pt) return true;
				return bool(promotion && promotion != (type_of(m) == PROMOTION));
			});
		if (list.size() == 0) return { "", MOVE_NONE };
		if (list.size() == 1) return { UCI::move(list[0],pos.is_chess960()), list[0] };

		// Disambiguating
		if (file >= FILE_A && file <= FILE_H) std::erase_if(list, [&](const auto& m) { return file_of(from_sq(m)) != file; });
		if (rank >= RANK_1 && rank <= RANK_8) std::erase_if(list, [&](const auto& m) { return rank_of(from_sq(m)) != rank; });
		if (list.size() == 1) return { UCI::move(list[0],pos.is_chess960()), list[0] };
		return { "", MOVE_NONE };
	}

	// Returns the Move from a string in LAN or SAN.
	Move AlgebraicToMove(const std::string& s, const Position& pos)
	{
		Square move_from, move_to;
		PieceType p = NO_PIECE_TYPE;
		Move move = MOVE_NONE;
		std::smatch match;

		// Test for LAN
		std::regex rx("([a-h][1-8])([a-h][1-8])([qrbn]?)");
		if (std::regex_match(s, match, rx))
		{
			move_from = get_square_from(match[1]);
			move_to = get_square_from(match[2]);

			// Handle promotion
			if (pos.piece_on(move_from) == PAWN && relative_rank(pos.side_to_move(), move_to) == RANK_8)
			{
				if (match[3].length())
				{
					char s = match[3].str()[0];
					if (s == 'q') p = QUEEN;
					else if (s == 'r') p = ROOK;
					else if (s == 'b') p = BISHOP;
					else if (s == 'n') p = KNIGHT;
				}
				else p = QUEEN;
				return make<PROMOTION>(move_from, move_to, p);
			}
			return make_move(move_from, move_to);
		}

		// Test for SAN.
		rx = R"(([NBRQK])?([a-h1-8])?([1-8])?([x])?([a-h][1-8])(\=[NBRQ])?.*)";
		if (std::regex_match(s, match, rx))
		{
			PieceType p;
			if (match[1].length())
			{
				switch (match[1].str()[0])
				{
				case 'N':
					p = KNIGHT;
					break;
				case 'B':
					p = BISHOP;
					break;
				case 'R':
					p = ROOK;
					break;
				case 'Q':
					p = QUEEN;
					break;
				default:
					p = KING;
				}
			}
			else p = PAWN;

			File amb_file = File(-1);
			Rank amb_rank = Rank(-1);
			if (match[2].length())
			{
				char c = match[2].str()[0];
				if (isdigit(c)) amb_rank = Rank(c - '1');
				else amb_file = File(c - 'a');
			}
			if (match[3].length())
			{
				char c = match[3].str()[0];
				amb_rank = Rank(c - '1');
			}
			move_to = get_square_from(match[5]);

			// Handle promotion.
			bool promotion = false;
			if (p == PAWN && relative_rank(pos.side_to_move(), move_to) == RANK_8)
			{
				PieceType prom;
				if (match[6].length())
				{
					char s = match[6].str()[1];
					if (s == 'q') prom = QUEEN;
					else if (s == 'r') prom = ROOK;
					else if (s == 'b') prom = BISHOP;
					else if (s == 'n') prom = KNIGHT;
				}
				else prom = QUEEN;
				promotion = true;
			}
			return get_move_from(pos, p, move_to, promotion, amb_file, amb_rank).second;
		}

		// Handle castling.
		if (s == "O-O" || s == "o-o" || s == "0-0")
		{
			move_from = relative_square(pos.side_to_move(), SQ_E1);
			move_to = relative_square(pos.side_to_move(), SQ_G1);
			return make<CASTLING>(move_from, move_to);
		}
		if (s == "O-O-O" || s == "o-o-o" || s == "0-0-0")
		{
			move_from = relative_square(pos.side_to_move(), SQ_E1);
			move_to = relative_square(pos.side_to_move(), SQ_C1);
			return make<CASTLING>(move_from, move_to);
		}
		return move;
	}

	// Returns a string from LAN or SAN.
	std::string AlgebraicToString(const std::string& s, const Position& pos)
	{
		Square move_to;
		std::string smove;
		std::smatch match;

		// Test for LAN.
		std::regex rx("([a-h][1-8])([a-h][1-8])([qrbn]?)");
		if (std::regex_match(s, match, rx)) return s;

		// Test for SAN.
		Move move = MOVE_NONE;
		rx = R"(([NBRQK])?([a-h1-8])?([1-8])?([x])?([a-h][1-8])(\=[NBRQ])?.*)";
		if (std::regex_match(s, match, rx))
		{
			PieceType p;
			if (match[1].length())
			{
				switch (match[1].str()[0])
				{
				case 'N':
					p = KNIGHT;
					break;
				case 'B':
					p = BISHOP;
					break;
				case 'R':
					p = ROOK;
					break;
				case 'Q':
					p = QUEEN;
					break;
				default:
					p = KING;
				}
			}
			else p = PAWN;

			File amb_file = File(-1);
			Rank amb_rank = Rank(-1);
			if (match[2].length())
			{
				char c = match[2].str()[0];
				if (isdigit(c)) amb_rank = Rank(c - '1');
				else amb_file = File(c - 'a');
			}
			if (match[3].length())
			{
				char c = match[3].str()[0];
				amb_rank = Rank(c - '1');
			}
			move_to = get_square_from(match[5]);

			// Handle promotion.
			bool promotion = false;
			if (p == PAWN && relative_rank(pos.side_to_move(), move_to) == RANK_8)
			{
				PieceType prom;
				if (match[6].length())
				{
					char s = match[6].str()[1];
					if (s == 'q') prom = QUEEN;
					else if (s == 'r') prom = ROOK;
					else if (s == 'b') prom = BISHOP;
					else if (s == 'n') prom = KNIGHT;
				}
				else prom = QUEEN;
				promotion = true;
			}
			return get_move_from(pos, p, move_to, promotion, amb_file, amb_rank).first;
		}

		// Handle castling.
		if (s == "O-O" || s == "o-o" || s == "0-0") return pos.side_to_move() == WHITE ? "e1g1" : "e8g8";
		if (s == "O-O-O" || s == "o-o-o" || s == "0-0-0") return pos.side_to_move() == WHITE ? "e1c1" : "e8c8";
		return "";
	}

	// Converts a move to a SAN string.
	std::string to_san(Move move, const Position& pos)
	{
		static const char* piece = "  NBRQK";

		if (!move) return "(none)";
		if (move == MOVE_NULL) return "0000";

		std::ostringstream SAN;

		if (type_of(move) == CASTLING) SAN << (from_sq(move) > to_sq(move) ? "O-O-O" : "O-O");
		else
		{
			MoveList<LEGAL> ml(pos);
			std::vector<ExtMove> list(ml.begin(), ml.end());
			PieceType pt = type_of(pos.moved_piece(move));

			if (pt != PAWN) SAN << piece[pt];
			std::erase_if(list, [&](const auto& m) { return to_sq(m) != to_sq(move) || type_of(pos.moved_piece(m)) != pt; });

			if (pt != PAWN && list.size() > 1)
			{
				if (std::ranges::count_if(list, [&](const auto& m) { return file_of(from_sq(move)) == file_of(from_sq(m)); }) == 1)
					SAN << char(file_of(from_sq(move)) + 'a');
				else if (std::ranges::count_if(list, [&](const auto& m) { return rank_of(from_sq(move)) == rank_of(from_sq(m)); }) == 1)
					SAN << char(rank_of(from_sq(move)) + '1');
				else SAN << from_sq(move);
			}

			if (pos.capture(move))
			{
				if (pt == PAWN) SAN << char(file_of(from_sq(move)) + 'a');
				SAN << 'x';
			}
			SAN << to_sq(move);
			if (type_of(move) == EN_PASSANT) SAN << "/e.p.";
			if (type_of(move) == PROMOTION) SAN << '=' << "  NBRQK"[promotion_type(move)];
		}

		StateListPtr sp(new std::deque<StateInfo>(1));
		Position copy;
		copy.set(pos.fen(), pos.side_to_move(), &sp->back(), nullptr);
		copy.do_move_fast(move, sp->emplace_back());
		if (copy.checkers())
			SAN << (MoveList<LEGAL>(copy).size() ? '+' : '#');
		return SAN.str();
	}

	std::string to_san(const Search::RootMove& rm, const Position& pos)
	{
		std::ostringstream SAN;
		StateListPtr sp(new std::deque<StateInfo>(1));
		Position copy;
		copy.set(pos.fen(), pos.is_chess960(), &sp->back(), nullptr);
		for (const auto& move : rm.pv)
		{
			if (!move) break;
			assert(MoveList<LEGAL>(copy).contains(move));

			SAN << " " << to_san(move, copy);
			copy.do_move_fast(move, sp->emplace_back());
		}
		return SAN.str();
	}

}
