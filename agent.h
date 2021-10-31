/**
 * Framework for 2048 & 2048-like Games (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games (TCG 2021)
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include "weight.h"
#include <fstream>

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables and a learning rate
 */
class player : public agent {
public:
	player(const std::string& args = "") : agent("name=dummy role=player " + args), alpha(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~player() {
		if (meta.find("save") != meta.end())
			save_weights(meta["save"]);
	}

	int extract_index(const board& after, int a, int b, int c, int d) {
		return  MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(a), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * std::min((int)after(b), MAX_INDEX - 1) + 
				MAX_INDEX * std::min((int)after(c), MAX_INDEX - 1) + 
				std::min((int)after(d), MAX_INDEX - 1);
	}

	int extract_index(const board& after, int a, int b, int c, int d, int e) {
		return  MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(a), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(b), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * std::min((int)after(c), MAX_INDEX - 1) + 
				MAX_INDEX * std::min((int)after(d), MAX_INDEX - 1) + 
				std::min((int)after(e), MAX_INDEX - 1);
	}

	int extract_index(const board& after, int a, int b, int c, int d, int e, int f) {
		return  MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(a), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(b), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * MAX_INDEX * std::min((int)after(c), MAX_INDEX - 1) + 
				MAX_INDEX * MAX_INDEX * std::min((int)after(d), MAX_INDEX - 1) + 
				MAX_INDEX * std::min((int)after(e), MAX_INDEX - 1) +
				std::min((int)after(f), MAX_INDEX - 1);
	}

	float estimate_value (const board& after) {
		float value = 0;
		board tmp = after;

		for (int i : {0, 1, 2, 3}) {
			switch (i)
			{
			case 0:
				break;
			case 1:
				tmp.rotate_left();
				tmp.rotate_left();
				break;
			case 2:
				tmp.rotate_left();
				break;
			case 3:
				tmp.rotate_right();
				break;
			default:
				break;
			}
			value += net[0][extract_index(tmp, 0, 1, 4, 5, 8, 9)];
			value += net[1][extract_index(tmp, 1, 2, 5, 6, 9, 10)];
			value += net[2][extract_index(tmp, 2, 6, 10, 14)];
			value += net[3][extract_index(tmp, 3, 7, 11, 15)];
			switch (i)
			{
			case 0:
				break;
			case 1:
				tmp.rotate_right();
				tmp.rotate_right();
				break;
			case 2:
				tmp.rotate_right();
				break;
			case 3:
				tmp.rotate_left();
				break;
			default:
				break;
			}			
		}

		return value;
	}

	virtual action take_action(const board& before) {
		int best_op = -1;
		int best_reward = -1;
		float best_value = -std::numeric_limits<float>::max();
		board best_after;
		for (int op : {0, 1, 2, 3}) {
			board after = before;
			int reward = after.slide(op);
			if (reward == -1)	continue;

			float value = estimate_value(after);
			if (value + reward >= best_value + best_reward) {
				best_reward = reward;
				best_value = value;
				best_op = op;
				best_after = after;
			}
		}
		if (best_op != -1) {
			history.push_back({best_reward, best_after});
		}
		return action::slide(best_op);
	}

	virtual void open_episode(const std::string& flag = "") {
		history.clear();
	}

	virtual void close_episode(const std::string& flag = "") {
		if (history.empty())	return;
		if (alpha == 0)	return;
		adjust_value(history[history.size() - 1].after, 0);
		for (int i = history.size() - 2; i >= 0; i--) {
			float target = history[i].reward + estimate_value(history[i + 1].after);
			adjust_value(history[i].after, target);
		}
	}

	void adjust_value(const board& after, float target) {
		float current = estimate_value(after);
		float error = target - current;
		float adjust = alpha * error;
		board tmp = after;

		for (int i : {0, 1, 2, 3}) {
			switch (i)
			{
			case 0:
				break;
			case 1:
				tmp.rotate_left();
				tmp.rotate_left();
				break;
			case 2:
				tmp.rotate_left();
				break;
			case 3:
				tmp.rotate_right();
				break;
			default:
				break;
			}
			net[0][extract_index(tmp, 0, 1, 4, 5, 8, 9)] += adjust;
			net[1][extract_index(tmp, 1, 2, 5, 6, 9, 10)] += adjust;
			net[2][extract_index(tmp, 2, 6, 10, 14)] += adjust;
			net[3][extract_index(tmp, 3, 7, 11, 15)] += adjust;
			switch (i)
			{
			case 0:
				break;
			case 1:
				tmp.rotate_right();
				tmp.rotate_right();
				break;
			case 2:
				tmp.rotate_right();
				break;
			case 3:
				tmp.rotate_left();
				break;
			default:
				break;
			}			
		}
	}	

protected:
	virtual void init_weights(const std::string& info) {
//		net.emplace_back(65536); // create an empty weight table with size 65536
//		net.emplace_back(65536); // create an empty weight table with size 65536
		net.emplace_back(MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX);
		net.emplace_back(MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX);
		net.emplace_back(MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX);
		net.emplace_back(MAX_INDEX * MAX_INDEX * MAX_INDEX * MAX_INDEX);
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
	float alpha;
	struct step{
		int reward;
		board after;
	};
	std::vector<step> history;
	int MAX_INDEX = 24;
};

/**
 * random environment
 * add a new random tile to an empty cell
 * 2-tile: 90%
 * 4-tile: 10%
 */
class rndenv : public random_agent {
public:
	rndenv(const std::string& args = "") : random_agent("name=random role=environment " + args),
		space({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }), popup(0, 9) {}

	virtual action take_action(const board& after) {
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;
			board::cell tile = popup(engine) ? 1 : 2;
			return action::place(pos, tile);
		}
		return action();
	}

private:
	std::array<int, 16> space;
	std::uniform_int_distribution<int> popup;
};

/**
 * dummy player
 * select a legal action randomly
 */
class dummy_player : public random_agent {
public:
	dummy_player(const std::string& args = "") : random_agent("name=dummy role=player " + args),
		opcode({ 0, 1, 2, 3 }) {
			mode = args;
		}

	virtual action take_action(const board& before) {
		std::shuffle(opcode.begin(), opcode.end(), engine);
		int best_op = -1;
		if (mode == "moron")	return action();

		if (mode == "score") {
			board::reward best_reward = 0;
			for (int op : opcode) {
				board::reward reward = board(before).slide(op);
				if (reward >= best_reward) {
					best_reward = reward;
					best_op = op;
				}
			}
		}
		else if(mode == "space") {
			int best_count = 0;
			for (int op : opcode) {
				board tmp_board = board(before);
				board::reward reward = tmp_board.slide(op);
				int count = tmp_board.num_empty();
				if (reward == -1)	continue;
				if (count >= best_count) {
					best_count = count;
					best_op = op;
				}
			}
		}
		else if (mode == "monotonic") {
			int best = 0;
			for (int op : opcode) {
				board tmp_board = board(before);
				board::reward reward = tmp_board.slide(op);
				if (reward == -1)	continue;
				if (reward + tmp_board.monotonic() >= best) {
					best = reward + tmp_board.monotonic();
					best_op = op;
				}
			}
		}
		else if (mode == "corner") {
			int best = 0;
			for (int op : opcode) {
				board tmp_board = board(before);
				board::reward reward = tmp_board.slide(op);
				if (reward == -1)	continue;
				if ((reward + tmp_board.corner_sum()) >= best) {
					best = reward + tmp_board.corner_sum();
					best_op = op;
				}
			}
		}
		else {
			for (int op : opcode) {
				board::reward reward = board(before).slide(op);
				if (reward != -1)	return action::slide(op);
			}
			return action();
		}

		if (best_op != -1)	return action::slide(best_op);
		else	return action();
	}

private:
	std::array<int, 4> opcode;
	std::string mode;
};
