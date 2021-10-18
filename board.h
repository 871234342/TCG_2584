/**
 * Framework for 2048 & 2048-like Games (C++ 11)
 * board.h: Define the game state and basic operations of the game of 2048
 *
 * Author: Theory of Computer Games (TCG 2021)
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <array>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

/**
 * array-based board for 2048
 *
 * index (1-d form):
 *  (0)  (1)  (2)  (3)
 *  (4)  (5)  (6)  (7)
 *  (8)  (9) (10) (11)
 * (12) (13) (14) (15)
 *
 */
class board {
public:
	typedef uint32_t cell;
	typedef std::array<cell, 4> row;
	typedef std::array<row, 4> grid;
	typedef uint64_t data;
	typedef int reward;

public:
	board() : tile(), attr(0) {}
	board(const grid& b, data v = 0) : tile(b), attr(v) {}
	board(const board& b) = default;
	board& operator =(const board& b) = default;

	operator grid&() { return tile; }
	operator const grid&() const { return tile; }
	row& operator [](unsigned i) { return tile[i]; }
	const row& operator [](unsigned i) const { return tile[i]; }
	cell& operator ()(unsigned i) { return tile[i / 4][i % 4]; }
	const cell& operator ()(unsigned i) const { return tile[i / 4][i % 4]; }

	data info() const { return attr; }
	data info(data dat) { data old = attr; attr = dat; return old; }

public:
	bool operator ==(const board& b) const { return tile == b.tile; }
	bool operator < (const board& b) const { return tile <  b.tile; }
	bool operator !=(const board& b) const { return !(*this == b); }
	bool operator > (const board& b) const { return b < *this; }
	bool operator <=(const board& b) const { return !(b < *this); }
	bool operator >=(const board& b) const { return !(*this < b); }

public:

	/**
	 * return i-th fibonacci number
	 */
	static int fibonacci (int i) {
		int fib[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 
			377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711, 28657,
			46368, 75025, 121393, 196418, 317811, 514229, 832040, 1346269,
			2178309, 3524578};
		return fib[i];
	} 

	/**
	 * return the index of the input fibonacci number
	 * return -1 if the input is not a fibonacci number
	 */
	static int r_fibonacci (int i) {
		int fib[] = {0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 
			377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711, 28657,
			46368, 75025, 121393, 196418, 317811, 514229, 832040, 1346269,
			2178309, 3524578};
		int n = sizeof(fib) / sizeof(fib[0]);

		auto itr = std::find(fib, fib + n, i);
		if (itr != std::end(fib)) {
			return std::distance(fib, itr);
		}
		else {
			return -1;
		}
	}

	/**
	 * return the length of an monotonic sequence
	 * return 1, 2, 3, or 4
	 */
	int monotonic() {
		int boundary[] = {0, 3};
		int directions[] = {1, -1};	// increase or decrease
		int max_length = 0;
		// row check
		for (int r = 0; r < 4; r++) {
			auto& row = tile[r];
			for (int direction : directions) {
				int length = 1;
				for (int c = 0; c < 3 ; c++) {
					if (int(row[c]) - int(row[c + 1]) == direction) {
						length++;
						if (length > max_length) max_length = length;
					}
					else	length = 1;
				}
			}
		}
		// column check
		for (int c = 0; c < 4; c++) {
			for (int direction : directions) {
				int length = 0;
				for (int r = 0; r < 3; r++) {
					if (int(tile[r][c]) - int(tile[r + 1][c]) == direction) {
						length++;
						if (length > max_length)	max_length = length;
					}
					else	length = 1;
				}
			}
		}
		return max_length;
	}

	/**
	 * return the number of empty tiles of the board
	 */
	int num_empty() {
		int count = 0;
		for (int r = 0; r < 4; r++) {
			auto& row = tile[r];
			for (int c = 0; c < 4; c++) {
				int tile = row[c];
				if (tile == 0)	count++;
			}
		}
		return count;
	}

	/**
	 * return the sum of four corners
	 */
	int corner_sum() {
		int sum = 0;
		for (int r : {0, 3}) {
			for (int c : {0, 3}) {
				sum += tile[r][c];
			}
		}
		return sum;
	}

	/**
	 * place a tile (index value) to the specific position (1-d form index)
	 * return 0 if the action is valid, or -1 if not
	 */
	reward place(unsigned pos, cell tile) {
		if (pos >= 16) return -1;
		if (tile != 1 && tile != 2) return -1;
		operator()(pos) = tile;
		return 0;
	}

	/**
	 * apply an action to the board
	 * return the reward of the action, or -1 if the action is illegal
	 */
	reward slide(unsigned opcode) {
		switch (opcode & 0b11) {
		case 0: return slide_up();
		case 1: return slide_right();
		case 2: return slide_down();
		case 3: return slide_left();
		default: return -1;
		}
	}

	reward slide_left() {
		board prev = *this;
		reward score = 0;
		for (int r = 0; r < 4; r++) {
			auto& row = tile[r];
			int top = 0, hold = 0;
			for (int c = 0; c < 4; c++) {
				int tile = row[c];
				if (tile == 0) continue;
				row[c] = 0;
				if (hold) {
					if (std::abs(tile - hold) == 1 || (tile == 1 && hold == 1)) {
						row[top++] = std::max(tile, hold) + 1;
						score += fibonacci(std::max(tile, hold) + 1);
						hold = 0;
					} else {
						row[top++] = hold;
						hold = tile;
					}
				} else {
					hold = tile;
				}
			}
			if (hold) tile[r][top] = hold;
		}
		return (*this != prev) ? score : -1;
	}
	reward slide_right() {
		reflect_horizontal();
		reward score = slide_left();
		reflect_horizontal();
		return score;
	}
	reward slide_up() {
		rotate_right();
		reward score = slide_right();
		rotate_left();
		return score;
	}
	reward slide_down() {
		rotate_right();
		reward score = slide_left();
		rotate_left();
		return score;
	}

	void transpose() {
		for (int r = 0; r < 4; r++) {
			for (int c = r + 1; c < 4; c++) {
				std::swap(tile[r][c], tile[c][r]);
			}
		}
	}

	void reflect_horizontal() {
		for (int r = 0; r < 4; r++) {
			std::swap(tile[r][0], tile[r][3]);
			std::swap(tile[r][1], tile[r][2]);
		}
	}

	void reflect_vertical() {
		for (int c = 0; c < 4; c++) {
			std::swap(tile[0][c], tile[3][c]);
			std::swap(tile[1][c], tile[2][c]);
		}
	}

	/**
	 * rotate the board clockwise by given times
	 */
	void rotate(int r = 1) {
		switch (((r % 4) + 4) % 4) {
		default:
		case 0: break;
		case 1: rotate_right(); break;
		case 2: reverse(); break;
		case 3: rotate_left(); break;
		}
	}

	void rotate_right() { transpose(); reflect_horizontal(); } // clockwise
	void rotate_left() { transpose(); reflect_vertical(); } // counterclockwise
	void reverse() { reflect_horizontal(); reflect_vertical(); }

public:
	friend std::ostream& operator <<(std::ostream& out, const board& b) {
		out << "+------------------------+" << std::endl;
		for (auto& row : b.tile) {
			out << "|" << std::dec;
			for (auto t : row) out << std::setw(6) << fibonacci(t);
			out << "|" << std::endl;
		}
		out << "+------------------------+" << std::endl;
		return out;
	}
	friend std::istream& operator >>(std::istream& in, board& b) {
		for (int i = 0; i < 16; i++) {
			while (!std::isdigit(in.peek()) && in.good()) in.ignore(1);
			in >> b(i);
			b(i) = r_fibonacci(b(i));
		}
		return in;
	}

private:
	grid tile;
	data attr;
};
