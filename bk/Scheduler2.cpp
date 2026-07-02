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
	vector<string> prev_day_excludes;
	string next_day_force = "";
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

			if (forced_job_repeats < forced->max_repeats) {
				return this->descr == forced->descr;
			}
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

	vector<int> repeat_count; // indexed by Turno::ID, O(1) repeat lookups

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

	void init_repeat_counts(size_t turn_count) {
		repeat_count.assign(turn_count, 0);
	}

	int get_repeats(Turno* turn) {
		if (!initialized || repeat_count.empty()) return 0;
		return repeat_count[turn->ID];
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

	void clear_day(enum days day) {
		Turno* t = this->turni[day].turno;
		if (t == nullptr) return;
		ore -= t->ore;
		this->turni[day].turno = nullptr;
	}

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
	vector<vector<int>> turn_count; // [day][turno_id] -> how many employees assigned that turno that day

	void init_turn_counts(size_t turn_total) {
		turn_count.assign(DAY_COUNT, vector<int>(turn_total, 0));
	}

	bool assign_day(size_t emp_index, enum days day, Turno* turno, bool constr) {
		Employee& emp = employees[emp_index];
		if (!emp.set_day(day, turno)) return false;
		if (!emp.repeat_count.empty()) emp.repeat_count[turno->ID]++;
		if (!turn_count.empty()) turn_count[day][turno->ID]++;
		if (constr) emp.turni[day].is_constr = true;
		return true;
	}

	void unassign_day(size_t emp_index, enum days day) {
		Employee& emp = employees[emp_index];
		Turno* t = emp.turni[day].turno;
		if (t == nullptr) return;
		if (!emp.repeat_count.empty()) emp.repeat_count[t->ID]--;
		if (!turn_count.empty()) turn_count[day][t->ID]--;
		emp.clear_day(day);
	}

	bool is_turn_already_taken(Turno* turno, enum days day) {
		return turn_count[day][turno->ID] + 1 > turno->max_people;
	}

	bool is_day_filled_correctly(enum days day, Turno** turn_list, int turn_length) {
		for (int i = 0; i < turn_length; i++) {
			if (turn_list[i]->facultative) continue;
			if (turn_count[day][turn_list[i]->ID] == 0) return false;
		}
		return true;
	}
};

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

// One shuffle buffer per possible recursion index. Since "index" strictly
// increases by exactly 1 along any single root-to-leaf path, at most one
// active stack frame ever owns a given index at a time, so these buffers
// can be reused across the whole search without reallocating per node.
vector<vector<int>> g_order_buffers;

bool recursion_step(Schedule& sched, int index, Turno** turn_list, int turn_length, int total_slots) {

	nodes_visited++;
	if (nodes_visited % 5000 == 0) {
		cout << "\rNodes explored: " << nodes_visited << " Curr_Recursion: " << index << " Max_recursion: " << max_index << "/" << total_slots << flush;
	}

	// Skip constrained slots transparently
	while (index < total_slots) {
		int emp_index = index / DAY_COUNT;
		int day = index % DAY_COUNT;
		if (!sched.employees[emp_index].turni[day].is_constr) break;
		index++;
	}

	if (index > max_index) {
		max_index = index;
		print_schedule(sched);
	}

	// Base case: all slots filled
	if (index == total_slots) {
		if (!sched.employees.back().is_emp_week_valid()) return false;
		return true;
	}

	int emp_index = index / DAY_COUNT;
	int day = index % DAY_COUNT;
	Employee* emp = &sched.employees[emp_index];

	// Week-completion check at employee boundary (covers min_ore and min_rest,
	// and fires regardless of whether the previous employee's slots were
	// filled by recursion or pre-set as CSV constraints)
	if (day == 0 && emp_index > 0) {
		if (!sched.employees[emp_index - 1].is_emp_week_valid()) return false;
	}

	vector<int>& order = g_order_buffers[index];
	static mt19937 rng(random_device{}());
	shuffle(order.begin(), order.end(), rng);

	Turno* prev_day = (day > 0) ? emp->turni[day - 1].turno : nullptr;

	int forced_job_repeats = 0;
	if (prev_day != nullptr && prev_day->next_day_force_turno != nullptr) {
		forced_job_repeats = emp->get_repeats(prev_day->next_day_force_turno);
	}

	if (prev_day != nullptr && prev_day->next_day_force != "" && day % 2 == 0 && forced_job_repeats < prev_day->next_day_force_turno->max_repeats) {
		return false;
	}

	for (int i = 0; i < turn_length; i++) {
		Turno* candidate = turn_list[order[i]];

		if (!candidate->is_job_assignable(prev_day, emp->get_repeats(candidate), forced_job_repeats)) {
			if (DEBUG) {
				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: is_job_assignable" << endl << endl << endl;
				print_schedule(sched);
			}
			continue;
		}
		if (sched.is_turn_already_taken(candidate, (enum days)day)) {
			if (DEBUG) {
				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: is_turn_already_taken" << endl << endl << endl;
				print_schedule(sched);
			}
			continue;
		}

		if (emp->ore + candidate->ore > emp->max_ore) {
			if (DEBUG) {
				cout << endl << endl << "Tried: " << candidate->descr << endl << "FAILED: max ore check" << endl << endl << endl;
				print_schedule(sched);
			}
			continue;
		}

		if (!sched.assign_day(emp_index, (enum days)day, candidate, false)) {
			continue;
		}

		bool ok = true;
		if (emp_index == (int)sched.employees.size() - 1) {
			if (!sched.is_day_filled_correctly((enum days)day, turn_list, turn_length)) {
				ok = false;
			}
		}

		if (ok && recursion_step(sched, index + 1, turn_list, turn_length, total_slots)) {
			return true;
		}

		sched.unassign_day(emp_index, (enum days)day);
	}

	return false;
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

		while (getline(config_file, line)) {

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

			string list = line.substr(line.find('[') + 1, line.find(']') - line.find('[') - 1);

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

	// Now that all Turno IDs exist, size up the O(1) lookup tables
	for (auto& emp : sched.employees) {
		emp.init_repeat_counts(turni.size());
	}
	sched.init_turn_counts(turni.size());

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
				for (size_t ei = 0; ei < sched.employees.size(); ei++) {
					if (sched.employees[ei].name == employee_name) {
						sched.assign_day(ei, (days)day, turno, true);
						break;
					}
				}
			}
		}

		csv.close();
	}

	int total_slots = DAY_COUNT * (int)sched.employees.size();

	g_order_buffers.assign(total_slots, vector<int>(turni.size()));
	for (auto& buf : g_order_buffers) {
		iota(buf.begin(), buf.end(), 0);
	}

	bool found = recursion_step(sched, 0, &turni[0], (int)turni.size(), total_slots);

	if (!found) {
		cout << "No solution found." << endl;
		return 1;
	}

	print_schedule(sched);

	return 0;

}