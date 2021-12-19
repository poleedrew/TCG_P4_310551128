/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
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
#include <fstream>
#include <time.h>

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
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	struct node{
		double value = 0;
		std::vector<node*> child_node;
		double nb = 0;
		action::place move;
		action::place op_move;
		double rate = 0;
	};
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if(args.find("N") != std::string::npos){
			simulation_count = std::stoull(meta.at("N")); //simulation count
			s_state = true;
		}
		else {
			simulation_count = 0;
			s_state = false;
		}
		if(args.find("T") != std::string::npos){
			time_threshold = std::stoull(meta.at("T")); //thinking time
			simulation_count = 1;
			t_state = true;
		}
		else {
			time_threshold = 1000000;
			t_state = false;
		}
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}
	bool check_win(board state, std::vector<action::place>& xx){
		
		for(const action::place& move: xx){	
			if(move.apply(state) == board::legal)return true;
		}
		return false;
	}
	action::place valid_move(board state, std::vector<action::place> xx){
		// std::shuffle(xx.begin(), xx.end(), engine);
		for(const action::place& move: xx){
			if(move.apply(state) == board::legal){
				return move;
			}
		}
		return action();
	}
	int simulation(board state, std::vector<action::place> aa, std::vector<action::place> op_aa){
		while(true){ // random board
			
			if(check_win(state, op_aa)){
				action::place move = valid_move(state, op_aa);
				move.apply(state);
			}
			else return 1;
			if(check_win(state, aa)){
				action::place move = valid_move(state, aa);
				move.apply(state);
			}
			else return 0;
			
		}
	}
	virtual action take_action(const board& state) {
		// selection process
		std::shuffle(space.begin(), space.end(), engine);
		if(simulation_count == 0){
			for(action::place& move:space){
				board after = state;
				if(move.apply(after) == board::legal)
					return move;
			}
			return action();
		}
		else{
			board::piece_type op = who==board::black? board::white:board::black;
			for (size_t i = 0; i < space.size(); i++)
				op_space.push_back(action::place(i, op));
			
			b_root = new node;
			b_root->value = 0;
			b_root->nb = 0;

			std::shuffle(op_space.begin(), op_space.end(), engine);

			for(const action::place& move : space){
				board after = state;
				if(move.apply(after) == board::legal){
					node* tmp = new node;
					tmp->move = move;
					tmp->nb = 0;
					b_root->child_node.push_back(tmp);
				}
			}
			if(b_root->child_node.size() == 1){
				return b_root->child_node.front()->move;
			}else if(b_root->child_node.size() == 0)
				return action();
		}
		double START, END;
		START = clock();
		END = clock();
		int cycle = simulation_count;
		
		while(true){
			if(s_state){
				if(cycle == 0)break;
				else cycle--;
			}
			if(t_state){
				if(END - START >= time_threshold -10){
					break;
				} 
				else END = clock();
			}
			board current_state = state;
			if(check_win(current_state, space) == false)	break;
			std::vector<node*> t;
			t.push_back(b_root);
			
			while(t.back()->child_node.size()){
				node* tmp = descendByUCB1(t.back());
				t.push_back(tmp);
			}
			if(t.back()->nb != 0 && check_win(current_state, space)){
				for(const action::place& move : space){
					board after = current_state;
					if(move.apply(after) == board::legal){
						node* child = new node;
						child->move = move;
						child->nb = 0;
						t.back()->child_node.push_back(child);
						break;
					}
				}
				t.push_back(descendByUCB1(t.back()));
			}

			for(size_t i=1;i<t.size(); i++){
				
				if(check_win(current_state, space) == false)
				break;
				t[i]->move.apply(current_state);
				if(i==t.size()-1) break;

				if(check_win(current_state, op_space) == false)
				break;
				if(t[i]->nb == 0){
					for(action::place& move : op_space){
						board after = current_state;
						if(move.apply(after) == board::legal){
							move.apply(current_state);
							t[i]->op_move = move;
							break;
						}
					}
				}else{
					t[i]->op_move.apply(current_state);
				}
			}
			int tmp;
			if(check_win(current_state, op_space) == false)
				tmp = 1;
			else
				tmp = simulation(current_state, space, op_space);
			updateValue(t, tmp);
			END = clock();
		}
		int index = 0; double max = 0;
		for(size_t i=0;i<b_root->child_node.size();i++){
			if(b_root->child_node[i]->nb >= max){
				max = b_root->child_node[i]->nb;
				index = i;
			}
		}
		return b_root->child_node[index]->move;
	}
	
	node* descendByUCB1(node* root){ // selection
		double nb = 0;
		for(size_t i = 0;i<root->child_node.size(); i++)
			nb += root->child_node[i]->nb;

		std::vector<double> v;
		for(size_t i=0; i<root->child_node.size();i++ ){
			if(root->child_node[i]->nb == 0){
				v.push_back(10000.0);
			}else{
				double result = root->child_node[i]->value / root->child_node[i]->nb + sqrt(2.4*log(nb)/root->child_node[i]->nb);
				v.push_back(result);
			}
		}
		int max = 0;
		int index = 0;
		for(size_t j=0; j < v.size();j++){
			if(v[j] >= max){
				max = v[j];
				index = j;
			}
		}
		return root->child_node[index];
	}
	void updateValue(std::vector<node*> mcts_tree, int value){ //back propagation
		for(int i=mcts_tree.size()-1; i>=0;i--){
			mcts_tree[i]->value = mcts_tree[i]->value + value;
			mcts_tree[i]->nb = mcts_tree[i]->nb + 1;
		}
	}

private:
	
	std::vector<action::place> space;
	board::piece_type who;
	size_t simulation_count = 0;
	int time_threshold = 0;
	std::vector<action::place> op_space;
	node* b_root;
	bool s_state = false;
	bool t_state = false;
};
