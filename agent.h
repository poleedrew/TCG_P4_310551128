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
		int value;
		int expand_count = 0;
		int vist_count = 1;
		std::vector<node*> child_node;
		int nb = 0;
		action::place move;
	};
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		// selection process
		if (simulation_count == 100){
			b_root = new node;
			w_root = new node;
			b_root->value = 0;
			b_root->nb = 0;
			w_root->value = 0;
			w_root->nb = 0;
			current = who==1?b_root:w_root;
			simulation_flag = true;
		}
		if(simulation_flag){ //simulation process
			for(const action::place& move : space){
				board after = state;
				if(move.apply(after) == board::legal){
					current->expand_count++;
				}
			}
			std::shuffle(space.begin(), space.end(), engine);
			
			action::place first_move;
			for (const action::place& move : space) {
				board after = state;
				if (move.apply(after) == board::legal){
					if(first){
						first_move = move;
						first = false;
					}
					return move;
				}
			}
			//first result occur
			/*
			if(who==1){ //I'm black 
				node* b = new node;
				b->move = first_move;
				b->nb = 0;
				b->value = 0; //lose
				// b_root->value = b->value;
				b_root->child_node.push_back(b);
				node* w = new node;
				w->move = first_move;
				w->nb = 0;
				w->value = 1; //win
				// w_root->value = w->value;
				w_root->child_node.push_back(w);
				playOneSequence(b_root);
				playOneSequence(w_root);
			}else{ // I'm white
				node* b = new node;
				b->move = first_move;
				b->nb = 0;
				b->value = 1; //lose
				// b_root->value = b->value;
				b_root->child_node.push_back(b);
				node* w = new node;
				w->move = first_move;
				w->nb = 0;
				w->value = 0; //win
				// w_root->value = w->value;
				w_root->child_node.push_back(w);
				playOneSequence(b_root);
				playOneSequence(w_root);
			}*/
			node *tmp = new node;
			tmp->nb = 0;
			tmp->value = 0;
			tmp->move = first_move;
			current->child_node.push_back(tmp);
			 
			playOneSequence(current); 
			simulation_flag = false;
			current = who==1?b_root:w_root;
			simulation_count--;
		}
		else{ // tree policy
			if(current->expand_count){ // still have the legal move can expand
				action::place new_move;
				for(const action::place& move: space){
					board after = state;
					if(move.apply(after) == board::legal){
						for(size_t i = 0; i < current->child_node.size(); i++){
							if(current->child_node[i]->move != move){
								node* tmp = new node;
								tmp->nb = 0;
								tmp->move = new_move;
								current->child_node.push_back(tmp);
								new_move = move;
								current->expand_count--;
								break;
							}
						}
						break;
					}
				}
				first = true;
				simulation_flag = true;
				return new_move;
			}else{
				std::vector<node*> t;
				t.push_back(current);
				int i = 0;
				while(t[i]->child_node.size()){
					t.push_back(descendByUCB1(t[i]));
					i = i + 1;
					current = t[i];
					board after = state;
					if(current->move.apply(after) == board::legal){
						return current->move;
					}
				}
				current = t[i];
				first = true;
				simulation_flag = true;
				return current->move;
			}
		}
		
		return action();
	}
	
	void playOneSequence(node* root) { //simulation
		std::vector<node*> t; t.push_back(root);
		int i = 0;
		while(t[i] -> child_node.size()){
			t.push_back(descendByUCB1(t[i]));
			i = i + 1;
		}
		updateValue(t, -t[i]->value);
		
	}
	node* descendByUCB1(node* tmp){ // selection
		int nb = 0;
		for(size_t i=0; i < tmp->child_node.size(); i++){
			nb += tmp->child_node[i]->nb;
		}
		std::vector<int> v;
		v.reserve(tmp->child_node.size());
		for(size_t i=0;i < tmp->child_node.size(); i++){
			
			if( tmp->child_node[i]->nb == 0){
				v[i] = 100000; //infinity
			}
			else{
				v[i] = -tmp->child_node[i]->value / tmp->child_node[i]->nb +sqrt(2*log(nb) / tmp->child_node[i]->nb );
			}
		}
		int index = 0, max = 0;
		for(size_t j=0;j < tmp->child_node.size(); j++){
			if(max > v[j]){
				max = v[j];
				index = j;
			}
		}
		return tmp->child_node[index];
	}
	void updateValue(std::vector<node*> mcts_tree, int value){ //back propagation
		for(int i=mcts_tree.size()-2; i>=0;i--){
			mcts_tree[i]->value = mcts_tree[i]->value + value; //may wrong
			mcts_tree[i]->nb = mcts_tree[i]->nb + 1;
			value = -value;
		}
	}

private:
	
	std::vector<action::place> space;
	board::piece_type who;

	std::vector<action::place> legal;
	int simulation_count = 100;
	// std::vector<node*> black_mcts_tree;
	// std::vector<node*> white_mcts_tree;
	bool simulation_flag = true;
	bool first = true;
	node* current;
	node* b_root;
	node* w_root;
};
