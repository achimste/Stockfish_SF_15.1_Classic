#ifndef SAN_H_INCLUDED
#define SAN_H_INCLUDED

#include "position.h"
#include "search.h"

namespace Stockfish::SAN {
	Move AlgebraicToMove(const std::string& s, const Position& pos);
	std::string AlgebraicToString(const std::string& s, const Position& pos);
	std::string to_san(Move, const Position& pos);
	std::string to_san(const Search::RootMove&, const Position& pos);
}

#endif
