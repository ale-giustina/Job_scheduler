
#include <iostream>
#include <vector>
#include <time.h>
#include <random>
#include <math.h>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>

using namespace std;

#define MAX_PEOPLE (20)
#define MAX_TURNS (20)
#define DEBUG (false)

class Employee;

enum days {
	MONDAY,
	TUESDAY,
	WEDNESDAY,
	THURSDAY,
	FRIDAY,
	SATURDAY,
	SUNDAY,
	DAY_COUNT
};

class Turno {

private:
	static int cum_id;

public:

	int ID;
	float ore = 0;
	string descr = "";
	vector<string> prev_day_excludes; //The jobs in this list will exclude the current slot being filled with this job if present the previous day
	string next_day_force = ""; //The job in this list will be assigned to the next day slot for this employee
	Turno* next_day_force_turno = nullptr;
	int max_repeats;
	bool facultative;
	int max_people;

	Turno(string description, float h, vector<string> pr_day_excludes, string nxt_day_force, int rep, bool fac, int max_p) : descr(description), ore(h), prev_day_excludes(pr_day_excludes), next_day_force(nxt_day_force), max_repeats(rep), facultative(fac), max_people(max_p) {
		this->ID = cum_id++;
		return;
	}

	bool is_job_assignable(Turno* prev_day, int repeats, int forced_job_repeats) {

		if (prev_day == NULL) return true;
		if (repeats >= max_repeats) return false;

		if (prev_day->next_day_force_turno != nullptr) {
			Turno* forced = prev_day->next_day_force_turno;

			// Hard force still applies only if the employee hasn't
			// already hit the forced job's own max_repeats.
			if (forced_job_repeats < forced->max_repeats) {
				return this->descr == forced->descr;
			}
			// Otherwise: soft limit reached, forcing no longer applies,
			// fall through to normal exclusion checks below.
		}

		for (const auto& i : prev_day_excludes) {
			if (i == prev_day->descr) return false;
		}

		return true;

	};

};
int Turno::cum_id = 0;

struct TurnoSlot {
	Turno* turno = nullptr;
	bool is_constr = false;
};

class Employee {
public:
	string name = "";
	float ore = 0;
	TurnoSlot turni[DAY_COUNT] = { {nullptr }, {nullptr}, {nullptr }, {nullptr},{nullptr }, {nullptr},{nullptr} };

	int min_ore = 38;
	int max_ore = 42;

	int min_rest = 0;
	string rest_descr = "";

	bool initialized = false;

	Employee() {};

	void set_name(string s) {
		this->name = s;
		initialized = true;
	}

	void change_hours(int min, int max, int min_rest, string rest_descr) {
		this->min_ore = min;
		this->max_ore = max;
		this->min_rest = min_rest;
		this->rest_descr = rest_descr;
	}

	int get_repeats(string turn) {

		if (!initialized) return false;
		int rep = 0;

		for (int i = 0; i < DAY_COUNT; i++) {
			if (this->turni[i].turno != nullptr) {

				if (this->turni[i].turno->descr == turn) rep++;

			}
		}

		return rep;

	};

	bool set_day(enum days day, Turno* turno) {
		if (!initialized) return false;
		if (this->turni[day].turno != nullptr) {
			ore -= this->turni[day].turno->ore;
		}
		ore += turno->ore;
		if (ore > max_ore) {
			ore -= turno->ore;
			return false;
		}
		this->turni[day].turno = turno;
		return true;

	};

	bool is_emp_week_valid() {
		if (this->ore < this->min_ore) {
			return false;
		}
		int count = 0;
		for (int i = 0; i < (int)DAY_COUNT; i++) {
			if (this->turni[i].turno->descr == this->rest_descr) count++;
		}
		if (count < this->min_rest) return false;
		return true;

	}

};

class Schedule {
public:
	vector<Employee> employees;

	bool is_turn_already_taken(Turno* turno, enum days day) {
		int count = 1;

		for (const auto& i : employees) {
			if (i.turni[day].turno == nullptr) continue;
			if (turno->ID == i.turni[day].turno->ID) count ++;
		}

		if (count > turno->max_people) return true;

		return false;
	}

	bool is_day_filled_correctly(enum days day, Turno** turn_list, int turn_length) {
		for (int i = 0; i < turn_length; i++) {
			if (turn_list[i]->facultative) continue;
			bool found = false;
			for (const auto& x : employees) {
				if (x.turni[day].turno != nullptr &&
					x.turni[day].turno->ID == turn_list[i]->ID) {
					found = true;
					break;
				}
			}
			if (!found) return false;
		}
		return true;
	}
};

struct Leaf {
	Leaf* prev_leaf = nullptr;
	Leaf** next_leafs = nullptr;
	int next_leaf_num = 0;
	Turno* turn_added = nullptr;
	bool dead_end = false;
	Schedule sched;
};

bool is_turn_in_next_leafs(Turno* turno, Leaf** next_leafs) {
	int i = 0;
	while (next_leafs[i] != nullptr) {
		if (turno->ID == next_leafs[i++]->turn_added->ID) return true;
	}
	return false;
}

// min and max are both inclusive
int randint(int min, int max) {
	static mt19937 rng(random_device{}());
	uniform_int_distribution<int> dist(min, max);
	return dist(rng);
}


void print_schedule(Schedule& sol) {
	string day_names[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

	cout << left << setw(12) << "Employee";
	for (int d = 0; d < DAY_COUNT; d++)
		cout << setw(14) << day_names[d];
	cout << "Hours" << endl;
	cout << string(110, '-') << endl;

	for (size_t i = 0; i < sol.employees.size(); i++) {
		Employee& emp = sol.employees[i];
		cout << left << setw(12) << emp.name;
		for (int d = 0; d < DAY_COUNT; d++) {
			string slot = emp.turni[d].turno ? emp.turni[d].turno->descr : "---";
			if (emp.turni[d].is_constr) slot += "*";
			cout << setw(14) << slot;
		}
		cout << emp.ore << "h" << endl;
	}
	cout << "* = pre-set constraint" << endl;
}

long long nodes_visited = 0;

int max_index = 0;

Leaf* recursion_step(Leaf* leaf, int index, Turno** turn_list, int turn_length) {
	
	int total_slots = DAY_COUNT * leaf->sched.employees.size();

	nodes_visited++;
	if (nodes_visited % 500 == 0) { // don't flood the console
		cout << "\rNodes explored: " << nodes_visited << " Curr_Recursion: "<< index << " Max_recursion: " << max_index << "/" << total_slots << flush;
	}

	if (index > max_index) {
		max_index = index;
		print_schedule(leaf->sched);
	}

	// Skip constrained slots transparently
	while (index < total_slots) {
		int emp_index = index / DAY_COUNT;
		int day = index % DAY_COUNT;
		if (!leaf->sched.employees[emp_index].turni[day].is_constr) break;
		index++;
	}

	// Base case: all slots filled
	if (index == total_slots) {
		if (!leaf->sched.employees.back().is_emp_week_valid()) {
			leaf->dead_end = true;
			return nullptr;
		}
		return leaf;
	}

	int emp_index = index / DAY_COUNT;
	int day = index % DAY_COUNT;
	Employee* emp = &leaf->sched.employees[emp_index];

	// Min hours check at employee boundary
	if (day == 0 && emp_index > 0) {
		Employee* prev_emp = &leaf->sched.employees[emp_index - 1];
		if (prev_emp->ore < prev_emp->min_ore) {
			leaf->dead_end = true;
			return nullptr;
		}
	}

	if (day == 0 && emp_index > 0) {
		Employee* prev_emp = &leaf->sched.employees[emp_index - 1];
		if (!prev_emp->is_emp_week_valid()) {
			leaf->dead_end = true;
			return nullptr;
		}
	}

	// Allocate next_leafs on first visit, zero-initialized for null sentinel
	if (leaf->next_leafs == nullptr)
		leaf->next_leafs = new Leaf * [turn_length+1]();

	// Shuffled turn order for fairness
	static mt19937 rng(random_device{}());
	vector<int> order(turn_length);
	iota(order.begin(), order.end(), 0);
	shuffle(order.begin(), order.end(), rng);

	Turno* prev_day = (day > 0) ? emp->turni[day - 1].turno : nullptr;

	int forced_job_repeats = 0;
	if (prev_day != nullptr && prev_day->next_day_force_turno != nullptr) {
		forced_job_repeats = emp->get_repeats(prev_day->next_day_force_turno->descr);
	}

	if (prev_day != nullptr && prev_day->next_day_force != "" && day % 2 == 0 && forced_job_repeats< prev_day->next_day_force_turno->max_repeats) {
		leaf->dead_end = true;
		return nullptr;
	}

	for (int i = 0; i < turn_length; i++) {
		Turno* candidate = turn_list[order[i]];

		// Skip already tried turns at this node
		if (is_turn_in_next_leafs(candidate, leaf->next_leafs)) {
			if (DEBUG) {
				
				cout << endl << endl << "Tried: "<< candidate->descr << endl << "FAILED: is_turn_in_next_leafs" << endl << endl << endl;
				
				print_schedule(leaf->sched);

			}
			continue;
		}

		// Constraint checks, cheapest first
		if (!candidate->is_job_assignable(prev_day, emp->get_repeats(candidate->descr), forced_job_repeats)) {
		
			if (DEBUG) {

				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: is_job_assignable" << endl << endl << endl;

				print_schedule(leaf->sched);

			}
			
			continue;
		}
		if (leaf->sched.is_turn_already_taken(candidate, (enum days)day)) {
		
			if (DEBUG) {

				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: is_turn_already_taken" << endl << endl << endl;

				print_schedule(leaf->sched);

			}
			
			continue;
		}

		if (emp->ore + candidate->ore > emp->max_ore) {
			if (DEBUG) {

				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: max ore check" << endl << endl << endl;

				print_schedule(leaf->sched);

			}
			continue;
		}

		// Create child leaf as a snapshot of current schedule
		Leaf* child = new Leaf();
		child->prev_leaf = leaf;
		child->next_leafs = nullptr;
		child->next_leaf_num = 0;
		child->turn_added = candidate;
		child->dead_end = false;
		child->sched = leaf->sched;
		if (!child->sched.employees[emp_index].set_day((enum days)day, candidate)) {
			delete child;
			continue;
		}

		if (emp_index == child->sched.employees.size() - 1) {
			if (!child->sched.is_day_filled_correctly((enum days)day, turn_list, turn_length)) {
				delete child;
				continue;
			}
		}

		leaf->next_leafs[leaf->next_leaf_num++] = child;

		Leaf* res = recursion_step(child, index + 1, turn_list, turn_length);
		if (res != nullptr) return res;
		// Child was a dead end, loop continues to next candidate
	}

	// All candidates exhausted at this node
	leaf->dead_end = true;
	return nullptr;
}


int main() {
	ifstream config_file("setup.txt");

	string line;

	Schedule sched;

	int empl_num = 0;

	vector<Turno*> turni;

	if (config_file.is_open()) {

		cout << "CARICANDO LAVORATORI ..." << endl;
		cout << left << setw(20) << "Nome"
			<< right << setw(12) << "Min Ore"
			<< right << setw(12) << "Max Ore" 
			<< right << setw(12) << "Min rest" 
			<< right << setw(12) << "Min descr" << endl;
		cout << string(44, '-') << endl;

		while (getline(config_file,line)) {

			if (line.substr(0, 7) == "[TURNI]") break;
			if (line[0] == '[') continue;

			int first_comma = line.find(',');
			if (first_comma == string::npos) continue;   // skip header/blank lines

			string name = line.substr(0, first_comma);

			int second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string min_hr = line.substr(first_comma + 1, second_comma - first_comma - 1);

			first_comma = second_comma;

			second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string max_hr = line.substr(first_comma + 1, second_comma - first_comma - 1);

			first_comma = second_comma;

			second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string min_rest = line.substr(first_comma + 1, second_comma - first_comma - 1);

			first_comma = second_comma;

			string rest_descr = line.substr(first_comma + 2, second_comma - first_comma - 1);

			cout << left << setw(20) << name
				<< right << setw(12) << stoi(min_hr)
				<< right << setw(12) << stoi(max_hr) 
				<< right << setw(12) << stoi(min_rest) 
				<< right << setw(12) << "\"" << rest_descr << "\"" << endl;

			sched.employees.push_back(Employee());
			sched.employees[empl_num].set_name(name);
			sched.employees[empl_num++].change_hours(stoi(min_hr), stoi(max_hr), stoi(min_rest), rest_descr);
		}

		cout << endl << endl << "CARICANDO TURNI ..." << endl;

		cout << left << setw(15) << "Turno ID"
			<< right << setw(10) << "Ore"
			<< right << setw(30) << "Escl. Prec."
			<< right << setw(20) << "Forzatura Succ."
			<< right << setw(10) << "Max Rip."
			<< right << setw(12) << "Max. Pers." 
			<< right << setw(10) << "Fac" << endl;

		// Stampa una linea di separazione della giusta lunghezza (circa 77 caratteri)
		cout << string(107, '-') << endl;


		

		while (getline(config_file, line)) {

			if (line.substr(0, 11) == "[ENDCONFIG]") {
				break;
			}
			if (line[0] == ':') continue;

			int first_comma = line.find(',');
			if (first_comma == string::npos) continue;   // skip header/blank lines

			string name = line.substr(0, first_comma);

			int second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string ore = line.substr(first_comma + 1, second_comma - first_comma - 1);
			
			float ore_num = stof(ore);

			vector<string> prev_ex;

			string list = line.substr(line.find('[')+1, line.find(']')-line.find('[')-1);

			first_comma = list.find(',');

			if (first_comma != string::npos) {

				prev_ex.push_back(list.substr(0, first_comma));

				second_comma = list.find(',', first_comma + 1);

				while (second_comma != string::npos) {

					prev_ex.push_back(list.substr(first_comma + 1, second_comma - first_comma - 1));

					first_comma = second_comma;

					second_comma = list.find(',', first_comma + 1);

				}

			}

			first_comma = line.find(']');
			if (first_comma == string::npos) continue;

			second_comma = line.find(',', first_comma + 2);
			if (second_comma == string::npos) continue;

			string forzatura = line.substr(first_comma + 3, second_comma - first_comma - 3);

			forzatura = (forzatura.find('-') == string::npos) ? forzatura : "";

			first_comma = second_comma;

			second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string ripetizioni_max = line.substr(first_comma + 1, second_comma - first_comma - 1);

			int ripetizioni_max_num = stoi(ripetizioni_max);

			first_comma = second_comma;
			second_comma = line.find(',', first_comma + 1);

			if (second_comma == string::npos) continue;

			string max_persone = line.substr(first_comma + 1, second_comma - first_comma - 1);
			
			int max_pers;

			max_pers = stoi(max_persone);

			first_comma = second_comma;
			second_comma = line.find(',', first_comma + 1);
			
			string fac = line.substr(first_comma + 1);
			bool facul;

			facul = (fac.find('1') != string::npos);

			
			string prev_ex_str = "";
			if (prev_ex.empty()) {
				prev_ex_str = "none";
			}
			else {
				for (size_t i = 0; i < prev_ex.size(); ++i) {
					prev_ex_str += prev_ex[i];
					if (i < prev_ex.size() - 1) {
						prev_ex_str += ", ";
					}
				}
			}

			Turno* curr_turno = new Turno(name, ore_num, prev_ex, forzatura, ripetizioni_max_num, facul, max_pers);

			turni.push_back(curr_turno);

			cout << left << setw(15) << name
				<< right << setw(10) << ore_num
				<< right << setw(30) << prev_ex_str
				<< right << setw(20) << forzatura
				<< right << setw(10) << ripetizioni_max_num
				<< right << setw(12) << max_pers 
				<< right << setw(10) << facul << endl;
		}

		config_file.close();
	}
	else {
		cerr << "Unable to open file" << endl;
	}

	for (auto t : turni) {
		if (t->next_day_force == "") continue;
		for (auto t2 : turni) {
			if (t2->descr == t->next_day_force) {
				t->next_day_force_turno = t2;
				break;
			}
		}
	}



	ifstream csv("test1.csv");

	if (csv.is_open()) {

		string line;

		// Skip header
		getline(csv, line);

		while (getline(csv, line)) {

			stringstream ss(line);

			vector<string> fields;
			string field;

			while (getline(ss, field, ',')) {
				fields.push_back(field);
			}

			//if (fields.size() < DAY_COUNT + 1)
			//	continue;

			string turno_name = fields[0];

			// Find the Turno*
			Turno* turno = nullptr;
			for (auto t : turni) {
				if (t->descr == turno_name) {
					turno = t;
					break;
				}
			}

			if (turno == nullptr)
				continue;

			// Monday..Sunday
			for (int day = 0; day < DAY_COUNT; day++) {

				if (day + 2 > fields.size()) continue;

				string employee_name = fields[day + 1];

				if (employee_name.empty())
					continue;

				// Find employee
				for (auto& emp : sched.employees) {

					if (emp.name == employee_name) {

						emp.set_day((days)day, turno);
						emp.turni[day].is_constr = true;

						break;
					}
				}
			}
		}

		csv.close();
	}




	Leaf root;
	root.sched = sched;
	Leaf* result = recursion_step(&root, 0, & turni[0], turni.size());

	if (result == nullptr) {
		cout << "No solution found." << endl;
		return 1;
	}

	print_schedule(result->sched);
	
	return 0;


}