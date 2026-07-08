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
#include <cstring>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

#define DEBUG (false)
#define MAX_ITER (50000)

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
	bool is_circular = false;
	bool reserved_for_force = false;

	Turno(string description, float h, vector<string> pr_day_excludes, string nxt_day_force, int rep, bool fac, int max_p) : descr(description), ore(h), prev_day_excludes(pr_day_excludes), next_day_force(nxt_day_force), max_repeats(rep), facultative(fac), max_people(max_p) {
		this->ID = cum_id++;
		return;
	}

	bool is_job_assignable(Turno* prev_day, int repeats, int forced_job_repeats, int day) {

		if (prev_day == NULL) return true;

		if (repeats >= max_repeats) return false;

		if (prev_day->next_day_force_turno != nullptr) {
			Turno* forced = prev_day->next_day_force_turno;

			if (forced_job_repeats < forced->max_repeats) {
				return this->descr == forced->descr;
			}
		}

		if (this->reserved_for_force && (enum days)day != MONDAY) return false;

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

	vector<int> repeat_count;

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
	vector<vector<int>> turn_count; // [day][turno_id] how many employees assigned that turno that day

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

bool check_feasibility(Schedule& sched, vector<Turno*>& turni) {

	bool ok = true;
	auto fail = [&](const string& msg) {
		cerr << "[FEASIBILITY] " << msg << endl;
		ok = false;
		};

	if (sched.employees.empty()) {
		fail("No employees were loaded (check setup.txt [ADDETTI] section / file path).");
	}
	if (turni.empty()) {
		fail("No turni were loaded (check setup.txt [TURNI] section / file path).");
	}
	if (ok == false) return false; // nothing else can be safely checked without these

	int emp_count = (int)sched.employees.size();

	// 1. Turno-level sanity: dangling forzatura references, and max_people
	//    bounds that no employee count could ever satisfy.
	float max_ore = 0;
	for (auto t : turni) {
		if (t->next_day_force != "" && t->next_day_force_turno == nullptr) {
			fail("Turno '" + t->descr + "' has next-day forzatura '" + t->next_day_force + "' which does not match any known turno ID.");
		}
		if (t->max_people < 1) {
			fail("Turno '" + t->descr + "' has max_people = " + to_string(t->max_people) + " (must be >= 1).");
		}
		if (t->max_repeats < 0) {
			fail("Turno '" + t->descr + "' has a negative max_repeats (" + to_string(t->max_repeats) + ").");
		}
		max_ore += t->ore * t->max_people * 7;
	}
	int min_ore_lav = 0;
	for (auto& e : sched.employees) {
		min_ore_lav += e.min_ore;
	}
	if (min_ore_lav > max_ore) {
		fail("There are not enough jobs to satisfy min ore constraint");
	}

	cout << max_ore;

	// 2. Every non-facultative turno must be coverable: on any given day at
	//    most one turno per employee can be worked, so the number of
	//    mandatory turni can never exceed the number of employees.
	int mandatory_count = 0;
	for (auto t : turni) {
		if (!t->facultative) mandatory_count++;
	}
	if (mandatory_count > emp_count) {
		fail("There are " + to_string(mandatory_count) + " mandatory (non-facultative) turni but only " +
			to_string(emp_count) + " employee(s); each day needs at least one employee per mandatory turno.");
	}

	// 3. Per employee checks.
	for (auto& emp : sched.employees) {
		if (emp.min_ore > emp.max_ore) {
			fail(emp.name + ": min_ore (" + to_string(emp.min_ore) + ") is greater than max_ore (" + to_string(emp.max_ore) + ").");
		}
		if (emp.min_rest > (int)DAY_COUNT) {
			fail(emp.name + ": min_rest (" + to_string(emp.min_rest) + ") exceeds the number of days in the week (" + to_string((int)DAY_COUNT) + ").");
		}

		bool rest_turno_exists = false;
		for (auto t : turni) {
			if (t->descr == emp.rest_descr) { rest_turno_exists = true; break; }
		}
		if (!rest_turno_exists) {
			fail(emp.name + ": rest turno '" + emp.rest_descr + "' does not match any known turno ID.");
		}

		// Hours already committed via CSV constraints must not already
		// exceed the employee's max_ore.
		if (emp.ore > emp.max_ore) {
			fail(emp.name + ": CSV pre-assigned hours (" + to_string(emp.ore) + ") already exceed max_ore (" + to_string(emp.max_ore) + ").");
		}

		// Count how many days are locked by CSV constraints, and how many
		// of those are already the rest turno, to see whether min_rest can
		// still be reached using the remaining free days.
		int constrained_days = 0;
		int constrained_rest_days = 0;
		for (int d = 0; d < (int)DAY_COUNT; d++) {
			if (emp.turni[d].is_constr) {
				constrained_days++;
				if (emp.turni[d].turno != nullptr && emp.turni[d].turno->descr == emp.rest_descr) {
					constrained_rest_days++;
				}
			}
		}
		int free_days = (int)DAY_COUNT - constrained_days;
		if (constrained_rest_days + free_days < emp.min_rest) {
			fail(emp.name + ": min_rest (" + to_string(emp.min_rest) + ") is unreachable; only " +
				to_string(constrained_rest_days) + " rest day(s) already set and " + to_string(free_days) +
				" free day(s) remaining.");
		}

		// Crude upper bound on achievable hours: constrained hours plus the
		// most expensive possible shift on every remaining free day.
		if (!turni.empty()) {
			float max_shift = 0;
			for (auto t : turni) max_shift = max(max_shift, t->ore);
			float max_possible_ore = emp.ore + free_days * max_shift;
			if (max_possible_ore < emp.min_ore) {
				fail(emp.name + ": min_ore (" + to_string(emp.min_ore) + ") is unreachable even if every remaining free day used the longest available shift (best case " +
					to_string(max_possible_ore) + "h).");
			}
		}
	}

	// 4. Per turno/day CSV over-assignment: max_people already exceeded by
	//    pre-set constraints before the solver even starts.
	if (!sched.turn_count.empty()) {
		for (int d = 0; d < (int)DAY_COUNT; d++) {
			for (auto t : turni) {
				int assigned = sched.turn_count[d][t->ID];
				if (assigned > t->max_people) {
					fail("Day " + to_string(d) + ": turno '" + t->descr + "' already has " + to_string(assigned) +
						" employee(s) assigned via CSV constraints, exceeding max_people (" + to_string(t->max_people) + ").");
				}
			}
		}
	}

	// 5. Weekly capacity of the rest turno must be able to satisfy the sum
	//    of every employee's min_rest requirement.
	{
		int total_min_rest_needed = 0;
		for (auto& emp : sched.employees) total_min_rest_needed += emp.min_rest;

		// Assume (as is typical) a single shared "rest" turno description;
		// find it and use its max_people * DAY_COUNT as weekly capacity.
		// Only meaningful when all employees share the same rest_descr.
		bool all_same_rest = true;
		for (auto& emp : sched.employees) {
			if (emp.rest_descr != sched.employees[0].rest_descr) { all_same_rest = false; break; }
		}
		if (all_same_rest && !sched.employees.empty()) {
			for (auto t : turni) {
				if (t->descr == sched.employees[0].rest_descr) {
					int weekly_capacity = t->max_people * (int)DAY_COUNT;
					if (total_min_rest_needed > weekly_capacity) {
						fail("Total weekly min_rest demand across all employees (" + to_string(total_min_rest_needed) +
							") exceeds the rest turno '" + t->descr + "' weekly capacity (" + to_string(t->max_people) +
							" slot(s)/day x " + to_string((int)DAY_COUNT) + " day(s) = " + to_string(weekly_capacity) + ").");
					}
					break;
				}
			}
		}
	}

	if (ok) {
		cout << "[FEASIBILITY] All checks passed." << endl;
	}
	else {
		cout << "[FEASIBILITY] One or more problems were found above; aborting before running the solver." << endl;
	}

	return ok;
}

// min and max are both inclusive
int randint(int min, int max) {
	static mt19937 rng(random_device{}());
	uniform_int_distribution<int> dist(min, max);
	return dist(rng);
}


void print_schedule(Schedule& sol) {
	string day_names[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

	cout << left << setw(15) << "Employee";
	for (int d = 0; d < DAY_COUNT; d++)
		cout << setw(14) << day_names[d];
	cout << "Hours" << endl;
	cout << string(110, '-') << endl;

	for (size_t i = 0; i < sol.employees.size(); i++) {
		Employee& emp = sol.employees[i];
		cout << left << setw(15) << emp.name;
		for (int d = 0; d < DAY_COUNT; d++) {
			string slot = emp.turni[d].turno ? emp.turni[d].turno->descr : "---";
			if (emp.turni[d].is_constr) slot += "*";
			cout << setw(14) << slot;
		}
		cout << emp.ore << "h" << endl;
	}
	cout << "* = pre-set constraint" << endl;
}


string csv_escape(const string& field) {
	bool needs_quoting = field.find(',') != string::npos ||
		field.find('"') != string::npos ||
		field.find('\n') != string::npos;
	if (!needs_quoting) return field;

	string escaped = "\"";
	for (char c : field) {
		if (c == '"') escaped += "\"\"";
		else escaped += c;
	}
	escaped += "\"";
	return escaped;
}

void save_schedule_csv(Schedule& sol, vector<Turno*>& turni, const string& filename) {
	string day_names[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

	ofstream out(filename);
	if (!out.is_open()) {
		cerr << "Unable to open '" << filename << "' for writing." << endl;
		return;
	}

	// --- 1. By turno ---
	out << "Turno";
	for (int d = 0; d < DAY_COUNT; d++) out << "," << day_names[d];
	out << "\n";

	for (auto t : turni) {
		out << csv_escape(t->descr);

		for (int d = 0; d < DAY_COUNT; d++) {
			vector<string> names;
			for (auto& emp : sol.employees) {
				if (emp.turni[d].turno == t) {
					string n = emp.name;
					if (emp.turni[d].is_constr) n += "*";
					names.push_back(n);
				}
			}

			string cell;
			for (size_t i = 0; i < names.size(); i++) {
				cell += names[i];
				if (i + 1 < names.size()) cell += "; ";
			}
			out << "," << csv_escape(cell);
		}
		out << "\n";
	}

	// --- blank separator line ---
	out << "\n";

	// --- 2. By employee ---
	out << "Employee";
	for (int d = 0; d < DAY_COUNT; d++) out << "," << day_names[d];
	out << ",Hours\n";

	for (auto& emp : sol.employees) {
		out << csv_escape(emp.name);
		for (int d = 0; d < DAY_COUNT; d++) {
			string slot = emp.turni[d].turno ? emp.turni[d].turno->descr : "";
			if (emp.turni[d].is_constr) slot += "*";
			out << "," << csv_escape(slot);
		}
		out << "," << emp.ore << "\n";
	}

	out.close();
	cout << "Schedule saved to '" << filename << "'." << endl;
}

bool is_next_day_compatible(Employee* emp, Turno* candidate, int day, int total_slots) {
	if (candidate->next_day_force_turno == nullptr) return true;
	if (day + 1 >= (int)DAY_COUNT) return true; // no next day (Sunday), nothing to check

	TurnoSlot& next_slot = emp->turni[day + 1];
	if (!next_slot.is_constr) return true; // next day is free, solver will handle it normally

	Turno* forced = candidate->next_day_force_turno;
	int forced_repeats_so_far = emp->get_repeats(forced); // NOTE: same repeats-budget caveat as before applies here

	if (forced_repeats_so_far >= forced->max_repeats) return true; // force already exhausted, no constraint

	return next_slot.turno == forced; // next day MUST already be the forced turno
}

// ---------------------------------------------------------------------
// Failure-reason instrumentation.
// ---------------------------------------------------------------------
struct FailureStats {
	long long emp_week_invalid = 0;          // is_emp_week_valid() failed (min_ore / min_rest)
	long long job_not_assignable = 0;        // Turno::is_job_assignable() rejected the candidate
	long long turn_already_taken = 0;        // max_people already reached for that turno/day
	long long max_ore_exceeded = 0;          // candidate would push employee over max_ore
	long long next_day_incompatible = 0;     // next-day forzatura constraint violated
	long long assign_day_failed = 0;         // low-level set_day() failed (redundant safety net on max_ore)
	long long day_not_filled_correctly = 0;  // a mandatory turno had 0 people on that day
	long long circular_prune = 0;            // circular forzatura early-prune

	void reset() {
		emp_week_invalid = 0;
		job_not_assignable = 0;
		turn_already_taken = 0;
		max_ore_exceeded = 0;
		next_day_incompatible = 0;
		assign_day_failed = 0;
		day_not_filled_correctly = 0;
		circular_prune = 0;
	}

	long long total() const {
		return emp_week_invalid + job_not_assignable + turn_already_taken + max_ore_exceeded +
			next_day_incompatible + assign_day_failed + day_not_filled_correctly + circular_prune;
	}

	void print(long long iterations) const {
		vector<pair<string, long long>> items = {
			{"Weekly hours/rest invalid (is_emp_week_valid)", emp_week_invalid},
			{"Job not assignable (exclusions/forzatura rules)", job_not_assignable},
			{"Turno slot already full (max_people reached)", turn_already_taken},
			{"Weekly max hours exceeded", max_ore_exceeded},
			{"Next-day forzatura incompatible", next_day_incompatible},
			{"assign_day rejected (max hours safety net)", assign_day_failed},
			{"Day not fully covered (mandatory turno missing)", day_not_filled_correctly},
			{"Circular forzatura early prune", circular_prune}
		};

		sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
			return a.second > b.second;
			});

		long long tot = total();

		cout << endl << "==================== FAILURE REPORT ====================" << endl;
		cout << "No solution found after " << iterations << " stalled iterations." << endl;
		cout << "Breakdown of rejection reasons encountered while searching (most frequent first):" << endl;
		for (auto& item : items) {
			double pct = (tot > 0) ? (100.0 * (double)item.second / (double)tot) : 0.0;
			cout << "  " << left << setw(50) << item.first
				<< right << setw(10) << item.second
				<< "  (" << fixed << setprecision(1) << pct << "%)" << endl;
		}
		if (tot > 0) {
			cout << endl << ">>> Most likely bottleneck: " << items.front().first
				<< " (" << items.front().second << " hits)" << endl;
		}
		cout << "==========================================================" << endl << endl;
	}
};

FailureStats g_fail_stats;

long long nodes_visited = 0;
int max_index = 0;

int iter_wout_record = 0;

vector<vector<int>> g_order_buffers;

// ---------------------------------------------------------------------
// Versioned "best solutions" folder.
//
// A snapshot is written ONLY when a phase produces a complete, fully
// valid schedule (see the phase loop in main()). max_index is kept only
// as a stall-detection heuristic (has the search made any progress
// lately?) and no longer triggers a save on its own.
// ---------------------------------------------------------------------
const string g_best_solutions_dir = "best_solutions";
int g_best_version = 0;

void save_phase_solution(Schedule& sched, vector<Turno*>& turni, int constraints_applied) {
	g_best_version++;
	string filename = g_best_solutions_dir + "/v" + to_string(g_best_version) + ".csv";
	save_schedule_csv(sched, turni, filename);
	cout << "Saved " << filename << " (" << constraints_applied << " constrained day(s) applied)." << endl;
}

bool recursion_step(Schedule& sched, int index, Turno** turn_list, int turn_length, int total_slots) {

	if (iter_wout_record > MAX_ITER) {
		return false;
	}

	nodes_visited++;
	if (nodes_visited % 5000 == 0) {
		cout << "\rNodes explored: " << nodes_visited << " Curr_Recursion: " << index << " Max_recursion: " << max_index << "/" << total_slots << flush;
	}

	// Skip constrained slots transparently
	while (index < total_slots) {
		int emp_index = index / DAY_COUNT;
		int day = index % DAY_COUNT;
		if (day == 0 && emp_index > 0) {
			if (!sched.employees[emp_index - 1].is_emp_week_valid()) {
				g_fail_stats.emp_week_invalid++;
				return false;
			}
		}
		if (!sched.employees[emp_index].turni[day].is_constr) break;
		index++;
	}

	if (index > max_index) {
		max_index = index;
		iter_wout_record = 0;
	}
	else {
		iter_wout_record++;
	}

	// Base case: all slots filled
	if (index == total_slots) {
		if (!sched.employees.back().is_emp_week_valid()) {
			g_fail_stats.emp_week_invalid++;
			return false;
		}
		return true;
	}

	int emp_index = index / DAY_COUNT;
	int day = index % DAY_COUNT;
	Employee* emp = &sched.employees[emp_index];

	if (day == 0 && emp_index > 0) {
		if (!sched.employees[emp_index - 1].is_emp_week_valid()) {
			g_fail_stats.emp_week_invalid++;
			return false;
		}
	}

	vector<int>& order = g_order_buffers[index];
	static mt19937 rng(random_device{}());
	shuffle(order.begin(), order.end(), rng);

	Turno* prev_day = (day > 0) ? emp->turni[day - 1].turno : nullptr;

	int forced_job_repeats = 0;
	if (prev_day != nullptr && prev_day->next_day_force_turno != nullptr) {
		forced_job_repeats = emp->get_repeats(prev_day->next_day_force_turno);
	}

	// without this the trenino circular checks would be extremely slow as they would not prune early.
	// if other force jobs have to be added this will have to be removed and rethinked
	// now is not the time, I have to sleep
	if (prev_day != nullptr && prev_day->is_circular && day % 2 == 0 && forced_job_repeats < prev_day->next_day_force_turno->max_repeats) {
		g_fail_stats.circular_prune++;
		return false;
	}

	for (int i = 0; i < turn_length; i++) {
		Turno* candidate = turn_list[order[i]];

		if (!candidate->is_job_assignable(prev_day, emp->get_repeats(candidate), forced_job_repeats, day)) {
			g_fail_stats.job_not_assignable++;
			continue;
		}
		if (sched.is_turn_already_taken(candidate, (enum days)day)) {
			g_fail_stats.turn_already_taken++;
			continue;
		}

		if (emp->ore + candidate->ore > emp->max_ore) {
			g_fail_stats.max_ore_exceeded++;
			continue;
		}

		if (!is_next_day_compatible(emp, candidate, day, total_slots)) {
			g_fail_stats.next_day_incompatible++;
			continue;
		}

		if (!sched.assign_day(emp_index, (enum days)day, candidate, false)) {
			g_fail_stats.assign_day_failed++;
			continue;
		}

		bool ok = true;
		if (emp_index == (int)sched.employees.size() - 1) {
			if (!sched.is_day_filled_correctly((enum days)day, turn_list, turn_length)) {
				g_fail_stats.day_not_filled_correctly++;
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

// ---------------------------------------------------------------------
// Constrained-day list, read from the CSV but NOT applied immediately.
// Each entry is one (turno, employee, day) cell from the constraints
// file. The phase loop in main() applies them one at a time, gradually,
// re-solving the whole schedule from scratch after each addition.
// ---------------------------------------------------------------------
struct ConstraintEntry {
	Turno* turno;
	size_t emp_index;
	int day;
};

// Properly clears a schedule back to "nothing assigned" - replaces the
// old memcpy-based reset, which was undefined behavior on a struct that
// holds std::vectors (Schedule/Employee). Plain copy-assignment (used in
// main()) is the correct, safe way to snapshot/restore now.
void reset_schedule(Schedule& sched) {
	for (auto& emp : sched.employees) {
		for (int d = 0; d < (int)DAY_COUNT; d++) {
			emp.turni[d].turno = nullptr;
			emp.turni[d].is_constr = false;
		}
		emp.ore = 0;
		fill(emp.repeat_count.begin(), emp.repeat_count.end(), 0);
	}
	for (auto& day_counts : sched.turn_count) {
		fill(day_counts.begin(), day_counts.end(), 0);
	}
}

void apply_constraint(Schedule& sched, const ConstraintEntry& c) {
	sched.assign_day(c.emp_index, (enum days)c.day, c.turno, true);
}


int main() {
	ifstream config_file("setup.csv");

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
			if (line[0] == ':') continue;

			int first_comma = line.find(',');
			if (first_comma == string::npos) continue;

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
			second_comma = line.find(',', first_comma + 1);
			if (second_comma == string::npos) continue;

			string rest_descr = line.substr(first_comma + 2, second_comma - first_comma - 2);

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

			first_comma = list.find('-');

			if (first_comma != string::npos) {

				prev_ex.push_back(list.substr(0, first_comma));

				second_comma = list.find('-', first_comma + 1);

				while (second_comma != string::npos) {

					prev_ex.push_back(list.substr(first_comma + 1, second_comma - first_comma - 1));

					first_comma = second_comma;

					second_comma = list.find('-', first_comma + 1);

				}
				prev_ex.push_back(list.substr(first_comma + 1));

			}
			else if (list.size() != 0) {

				prev_ex.push_back(list);

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

	for (auto t : turni) {
		if (t->next_day_force_turno != nullptr &&
			t->next_day_force_turno->next_day_force_turno == t) {
			t->is_circular = true;
		}
	}

	for (auto t : turni) {
		if (t->next_day_force_turno != nullptr && !t->is_circular) {
			t->next_day_force_turno->reserved_for_force = true;
		}
	}

	// Now that all Turno IDs exist, size up the O(1) lookup tables
	for (auto& emp : sched.employees) {
		emp.init_repeat_counts(turni.size());
	}
	sched.init_turn_counts(turni.size());

	// ------------------------------------------------------------------
	// Read the constrained-days CSV into a flat list WITHOUT applying it.
	// The phase loop below applies these one at a time, gradually.
	// ------------------------------------------------------------------
	vector<ConstraintEntry> g_all_constraints;

	ifstream csv("test2.csv");

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

			if (fields.empty()) continue;

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

				if (day + 2 > (int)fields.size()) continue;

				string employee_name = fields[day + 1];

				if (employee_name.empty())
					continue;

				// Find employee
				for (size_t ei = 0; ei < sched.employees.size(); ei++) {
					if (sched.employees[ei].name == employee_name) {
						g_all_constraints.push_back({ turno, ei, day });
						break;
					}
				}
			}
		}

		csv.close();
	}

	cout << endl << "Loaded " << g_all_constraints.size() << " constrained-day entrie(s) from test2.csv." << endl;
	cout << "Solving gradually: phase 0 has zero constraints, then one constrained day is added per phase." << endl << endl;

	// Fresh folder for this run's successful-phase snapshots (v1.csv, v2.csv, ...).
	fs::create_directories(g_best_solutions_dir);
	g_best_version = 0;

	int total_slots = DAY_COUNT * (int)sched.employees.size();

	g_order_buffers.assign(total_slots, vector<int>(turni.size()));
	for (auto& buf : g_order_buffers) {
		iota(buf.begin(), buf.end(), 0);
	}

	const int MAX_STALL_ROUNDS = 200; // random restarts allowed per phase before giving up on it

	int last_successful_phase = -1;
	Schedule last_good_schedule;
	bool have_good_schedule = false;

	for (int applied = 0; applied <= (int)g_all_constraints.size(); applied++) {

		reset_schedule(sched);
		for (int i = 0; i < applied; i++) {
			apply_constraint(sched, g_all_constraints[i]);
		}

		cout << "==================== PHASE " << applied << " ("
			<< applied << " constrained day(s) applied) ====================" << endl;

		if (!check_feasibility(sched, turni)) {
			cout << "Phase " << applied << " is infeasible by the static checks above; stopping here." << endl;
			break;
		}

		Schedule sched_baseline = sched; // safe deep copy (proper vector copy, no memcpy)

		max_index = 0;
		iter_wout_record = 0;

		bool found = false;
		int stall_rounds = 0;

		while (!found && stall_rounds < MAX_STALL_ROUNDS) {

			g_fail_stats.reset();

			found = recursion_step(sched, 0, &turni[0], (int)turni.size(), total_slots);

			if (!found) {
				stall_rounds++;
				cout << " No solution found (restart " << stall_rounds << "/" << MAX_STALL_ROUNDS << ")." << endl;
				sched = sched_baseline; // restore this phase's starting point (constraints intact)
				iter_wout_record = 0;
				max_index = 0;
			}
		}

		if (!found) {
			cout << "Could not find a full solution for phase " << applied << " after "
				<< MAX_STALL_ROUNDS << " restart attempts. Stopping; the last saved snapshot ("
				<< "v" << g_best_version << ".csv) is the deepest gradually-constrained solution found." << endl;
			g_fail_stats.print(iter_wout_record);
			sched = sched_baseline;
			break;
		}

		cout << "SUCCESS on phase " << applied << "." << endl;
		print_schedule(sched);
		save_phase_solution(sched, turni, applied);
		last_successful_phase = applied;
		last_good_schedule = sched;
		have_good_schedule = true;
	}

	if (have_good_schedule) {
		cout << endl << "Best successful phase: " << last_successful_phase << " constrained day(s), saved as v"
			<< g_best_version << ".csv in " << g_best_solutions_dir << "/." << endl;
		save_schedule_csv(last_good_schedule, turni, "schedule_output.csv");
	}
	else {
		cout << endl << "No phase produced a successful schedule (not even with zero constraints)." << endl;
	}

	return 0;

}