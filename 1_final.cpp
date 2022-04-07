#include <bits/stdc++.h>
using namespace std;
typedef long long LL;
const int kMaxServerType = 100 + 5;
const int kMaxVrMachineType = 1000 + 5;
const int kMaxVrMachine = 100000 + 5;
const int kMaxDay = 1000 + 5;
const bool DEBUG = true;
const bool SCOREMIGRATE = true;
const bool BACKOFF = false;
const bool SORTREQUEST = true;
const bool SORTMIGRATE = false;
const int kSERVERFORDEL = 0;
typedef long long LL;
string GetString(char end) {
	char ch;
	string ret = "";
	while (cin >> ch) {
		if (ch == end) {
			return ret;
		}
		ret.push_back(ch);
	}
	return ret;
}
struct ServerType {
	string type;
	int core;
	int mem;
	LL hardware_cost;
	LL energe_cost;

	string Input(int id) {
		char ch;
		cin >> ch;
		type = GetString(',');
		cin >> core >> ch >> mem >> ch >> hardware_cost >> ch >> energe_cost >> ch;
		return type;
	}
};
struct VrMachineType {
	string type;
	int core;
	int mem;
	bool double_node;

	string Input(int id) {
		char ch;
		cin >> ch;
		type = GetString(',');
		cin >> core >> ch >> mem >> ch >> double_node >> ch;
		return type;
	}
};
struct Request {
	string op;
	string type;
	string id;
	int day;
	int del_day;
	int idx;
	void Input() {
		char ch;
		cin >> ch;
		op = GetString(',');
		if (op == "add") {
			type = GetString(',');
			id = GetString(')');
		} else {
			id = GetString(')');
		}
	}
};

struct DayRequest {
	vector<Request> requests;
};
struct GlobalVariable {
	int nMaxServerType;
	int nMaxVrMachineType;
	int nMaxDay;
	int nMaxRequest;
	int nMaxPreDay;
	ServerType server_type[kMaxServerType];
	VrMachineType vr_machine_type[kMaxVrMachineType];
	map<string, int> server_type2int, vr_machine_type2int;
	int one_server_type, target_server_type[kMaxVrMachineType];
	map<string, pair<int, int> > request_id2day_idx;
	int ratio_core = 1, ratio_mem = 1;
	double target_server_ratio = 1.3;
	int migrate_threshold = 300;
	int swap_vm_threshold = 300;
	int swap_vm_loop_times = 400000;
} input_variable;
struct Deploy {
	int server_id;
	int node;
	int idx;
	Deploy(int server_id, int node, int idx) {
		this->server_id = server_id;
		this->node = node;
		this->idx = idx;
	}
};
struct Decision {
	vector<pair<string, int> > purchase;
	vector<tuple<string, int, char> > migration;
	vector<Deploy> deploy;
	void clear() {
		purchase.clear();
		migration.clear();
		deploy.clear();
	}
};
struct Server {
	int type;
	int id;
	int core[2];
	int mem[2];
	vector<int> vr_machine_id;
	int purchase_day;
	int del_day;
	bool del_tmp;
};
struct VrMachine {
	int type;
	int id;
	int server_id;
	int node;
	bool del;
	int del_day;
};
struct Status {
	vector<string> vr_machine_int2id;
	vector<Server> server_list;
	vector<VrMachine> vr_machine_list;
	map<string, int> vr_machine_id2int;
	vector<Request> requests;
	int purchase_decision[kMaxServerType];
	int migration_max;
	LL score;
	void clear() {
		server_list.clear();
		vr_machine_list.clear();
		vr_machine_id2int.clear();
		vr_machine_int2id.clear();
		requests.clear();
		score = 0;
	}
};

struct Process {
	DayRequest day_request[kMaxDay];
	Decision decision[kMaxDay];
	Decision today_decision;
	Status pre_status, cur_status;

	int PurchaseServer(int server_id, int day, Status& status) {
		status.score += input_variable.server_type[server_id].hardware_cost;
		Server server;
		server.type = server_id;
		server.id = status.server_list.size();
		for (int node = 0; node < 2; node++) {
			server.core[node] = input_variable.server_type[server_id].core / 2;
			server.mem[node] = input_variable.server_type[server_id].mem / 2;
		}
		server.vr_machine_id.clear();
		server.purchase_day = day;
		server.del_tmp = false;
		status.server_list.push_back(server);
		status.purchase_decision[server_id]++;
		return server.id;
	}

	void PutVMInServer(VrMachine &vm, Server &server, int node, Request& request) {
		VrMachineType vm_type = input_variable.vr_machine_type[vm.type];
		if (vm_type.double_node) {
			for (int i = 0; i < 2; i++) {
				server.core[i] -= vm_type.core / 2;
				server.mem[i] -= vm_type.mem / 2;
			}
			vm.node = 0;
			today_decision.deploy.push_back(Deploy(vm.server_id, -1, request.idx));
		} else {
			server.core[node] -= vm_type.core;
			server.mem[node] -= vm_type.mem;
			vm.node = node;
			today_decision.deploy.push_back(Deploy(vm.server_id, node, request.idx));
		}
		server.vr_machine_id.push_back(vm.id);
	}

	// ret = 0:node 0 ok, ret = 1:node 1 ok, ret = 2: both nodes ok, ret = -1: neither ok
	inline int PutCheck(VrMachineType& vm_type, Server& server) {
		int ret = 0;
		if (vm_type.double_node) {
			if (server.core[0] >= vm_type.core / 2 && server.core[1] >= vm_type.core / 2 &&
				server.mem[0] >= vm_type.mem / 2 && server.mem[1] >= vm_type.mem / 2) {
					ret = 1;
				}
		} else {
			for (int node = 0; node < 2; node++) {
				if (server.core[node] >= vm_type.core && server.mem[node] >= vm_type.mem) {
					ret += node + 1;
				}
			}
		}
		return ret - 1;
	}

	void AddVM(Request& request, Status& status) {
		VrMachine vm;
		vm.del = false;
		vm.type = input_variable.vr_machine_type2int[request.type];
		vm.id = status.vr_machine_list.size();
		status.vr_machine_int2id.push_back(request.id);
		status.vr_machine_id2int[request.id] = vm.id;
		VrMachineType vm_type = input_variable.vr_machine_type[vm.type];
		bool find_server = false;
		for (int i = 0; i < status.server_list.size(); i++) {
			Server& server = status.server_list[i];
			int node = PutCheck(vm_type, server);
			if (node != -1) {
				vm.server_id = i;
				node %= 2;
				PutVMInServer(vm, server, node, request);
				find_server = true;
				break;
			}
		}
		if (!find_server) {
			int tmp_server_type_id = -1;
			tmp_server_type_id = input_variable.target_server_type[vm.type];
			// tmp_server_type_id = input_variable.one_server_type;
			vm.server_id = PurchaseServer(tmp_server_type_id, request.day, status);
			PutVMInServer(vm, status.server_list[vm.server_id], 0, request);
		}
		status.vr_machine_list.push_back(vm);
	}
	inline void ScoreServerWeight(Server& server, VrMachineType& vm_type, int* server_score, bool is_mig) {
		int score_0 = 1e9, score_1 = 1e9;
		if (vm_type.double_node) {
			if (server.core[0] >= vm_type.core / 2 && server.core[1] >= vm_type.core / 2 &&
				server.mem[0] >= vm_type.mem / 2 && server.mem[1] >= vm_type.mem / 2) {
					score_0 = input_variable.ratio_core * (server.core[0] + server.core[1] - vm_type.core) + 
							input_variable.ratio_mem * (server.mem[0] + server.mem[1] - vm_type.mem);
				}
		} else {
			if (server.core[0] >= vm_type.core && server.mem[0] >= vm_type.mem) {
				score_0 = (input_variable.ratio_core * (server.core[0] - vm_type.core) + input_variable.ratio_mem * (server.mem[0] - vm_type.mem));
			}
			if (server.core[1] >= vm_type.core && server.mem[1] >= vm_type.mem) {
				score_1 = (input_variable.ratio_core * (server.core[1] - vm_type.core) + input_variable.ratio_mem * (server.mem[1] - vm_type.mem));
			}
		}
		server_score[0] = score_0;
		server_score[1] = score_1;
		if (server.vr_machine_id.size() == 0 && !is_mig) {
			server_score[0] += 10000000;
			server_score[1] += 10000000;
		}
	}
	void AddVMScored(Request& request, Status& status, int day, double next_mem_core_ratio) {
		VrMachine vm;
		vm.del_day = -1;
		vm.del = false;
		vm.type = input_variable.vr_machine_type2int[request.type];
		vm.id = status.vr_machine_list.size();
		status.vr_machine_int2id.push_back(request.id);
		status.vr_machine_id2int[request.id] = vm.id;
		VrMachineType vm_type = input_variable.vr_machine_type[vm.type];

		int server_score[100000 + 5][2] = {};
		for (int i = 0; i < status.server_list.size(); i++) {
			Server& server = status.server_list[i];
			ScoreServerWeight(server, vm_type, server_score[i], false);
		}

		int find_server = -1, node = 0;
		int min_score = 1e9;

		if (request.del_day == day)
		for (int i = 0; i < kSERVERFORDEL; i++) {
			// if (status.server_list[i].del_tmp != request.del_day && status.server_list[i].vr_machine_id.size() != 0) {
			// 	continue;
			// }
			for (int j = 0; j < 2; j++) {
				if (server_score[i][j] != -1 && min_score > server_score[i][j]) {
					min_score = server_score[i][j];
					find_server = i;
					node = j;
				}
			}
		}

		if (find_server == -1)
		for (int i = kSERVERFORDEL; i < status.server_list.size(); i++) {
			// if (status.server_list[i].del_tmp != request.del_day && status.server_list[i].vr_machine_id.size() != 0) {
			// 	continue;
			// }
			for (int j = 0; j < 2; j++) {
				if (server_score[i][j] != -1 && min_score > server_score[i][j]) {
					min_score = server_score[i][j];
					find_server = i;
					node = j;
				}
			}
		}

		if (find_server != -1) {
			// if (status.server_list[find_server].vr_machine_id.size() == 0) {
			// 	status.server_list[find_server].del_day = request.del_day;
			// }
			vm.server_id = find_server;
			PutVMInServer(vm, status.server_list[vm.server_id], node, request);
		} else {
			int tmp_server_type_id = -1;

			// double cost = 1e9;
			// for (int i = 0; i < input_variable.nMaxServerType; i++) {
			// 	if (vm_type.double_node) {
			// 		if (input_variable.server_type[i].core < vm_type.core || input_variable.server_type[i].mem < vm_type.mem) {
			// 			continue;
			// 		}
			// 	} else if (input_variable.server_type[i].core / 2 < vm_type.core || input_variable.server_type[i].mem / 2 < vm_type.mem) {
			// 		continue;
			// 	}
			// 	double tmp_cost = input_variable.server_type[i].hardware_cost +
			// 					  input_variable.server_type[i].energe_cost * (input_variable.nMaxDay - request.day);
			// 	if (cost > tmp_cost) {
			// 		cost = tmp_cost;
			// 		tmp_server_type_id = i;
			// 	}
			// }

			tmp_server_type_id = input_variable.target_server_type[vm.type];
			// tmp_server_type_id = input_variable.one_server_type;
			vm.server_id = PurchaseServer(tmp_server_type_id, request.day, status);
			status.server_list[vm.server_id].del_day = request.del_day;
			PutVMInServer(vm, status.server_list[vm.server_id], 0, request);
		}
		status.vr_machine_list.push_back(vm);
	}
	void DelVM(Request& request, Status& status) {
		int vr_machine_id = status.vr_machine_id2int[request.id];
		VrMachine& vm = status.vr_machine_list[vr_machine_id];
		status.vr_machine_list[vr_machine_id].del = true;
		Server& server = status.server_list[vm.server_id];
		VrMachineType vm_type = input_variable.vr_machine_type[vm.type];
		if (vm_type.double_node) {
			for (int node = 0; node < 2; node++) {
				server.core[node] += vm_type.core / 2;
				server.mem[node] += vm_type.mem / 2;
			}
		} else {
			server.core[vm.node] += vm_type.core;
			server.mem[vm.node] += vm_type.mem;
		}
		for (vector<int>::iterator it = server.vr_machine_id.begin(); it != server.vr_machine_id.end(); it++) {
			if (*it == vr_machine_id) {
				server.vr_machine_id.erase(it);
				break;
			}
		}
	}
	void CalPurchase(Status& status) {
		for (int i = 0; i < input_variable.nMaxServerType; i++) {
			if (status.purchase_decision[i]) {
				today_decision.purchase.push_back(make_pair(input_variable.server_type[i].type, status.purchase_decision[i]));
			}
		}
	}
	void MoveVm(VrMachine& vm, Server& target_server, int node, Status& status) {
		Server& move_server = status.server_list[vm.server_id];
		VrMachineType vm_type = input_variable.vr_machine_type[vm.type];
		if (vm_type.double_node) {
			for (int i = 0; i < 2; i++) {
				move_server.core[i] += vm_type.core / 2;
				move_server.mem[i] += vm_type.mem / 2;
			}
		} else {
			move_server.core[vm.node] += vm_type.core;
			move_server.mem[vm.node] += vm_type.mem;
		}
		if (vm_type.double_node) {
			for (int i = 0; i < 2; i++) {
				target_server.core[i] -= vm_type.core / 2;
				target_server.mem[i] -= vm_type.mem / 2;
			}
			today_decision.migration.push_back(make_tuple(status.vr_machine_int2id[vm.id], target_server.id, '0'));
		} else {
			target_server.core[node] -= vm_type.core;
			target_server.mem[node] -= vm_type.mem;
			today_decision.migration.push_back(make_tuple(status.vr_machine_int2id[vm.id], target_server.id, 'A' + node));
		}
		for (vector<int>::iterator it = move_server.vr_machine_id.begin();
				it != move_server.vr_machine_id.end(); it++) {
					if (*it == vm.id) {
						move_server.vr_machine_id.erase(it);
						break;
					}
				}
		vm.server_id = target_server.id;
		vm.node = node;
		target_server.vr_machine_id.push_back(vm.id);
	}
	static inline bool cmpServerId(pair<int, Server> a, pair<int, Server> b) {
		int sources_a = input_variable.server_type[a.second.type].core + input_variable.server_type[a.second.type].mem -
							a.second.core[0] - a.second.core[1] - a.second.mem[0] - a.second.mem[1];
		int sources_b = input_variable.server_type[b.second.type].core + input_variable.server_type[b.second.type].mem -
							b.second.core[0] - b.second.core[1] - b.second.mem[0] - b.second.mem[1];
		int rest_sources_a = 0, rest_sources_b = 0;
		for (int i = 0; i < 2; i++) {
			rest_sources_a += a.second.core[i] + a.second.mem[i];
			rest_sources_b += b.second.core[i] + b.second.mem[i];
		}
		return rest_sources_a > rest_sources_b;
		// return sources_b < sources_a;
	}
	void SortServerMigrate(Status& status, int day) {
		for (int times = 0; times < 1; times++) {
			vector<pair<int, Server>> server_sort_id;
			server_sort_id.clear();
			for (int i = 0; i < status.server_list.size(); i++) {
				if (status.server_list[i].vr_machine_id.size() == 0) {
					continue;
				}
				server_sort_id.push_back(make_pair(i, status.server_list[i]));
			}

			sort(server_sort_id.begin(), server_sort_id.end(), cmpServerId);

			int cannot_migrate[kMaxVrMachineType] = {};
			for (int i = 0; i < server_sort_id.size(); i++) {
				int server_id = server_sort_id[i].first;
				Server& cur_server = status.server_list[server_id];
				for (int j = 0; j < cur_server.vr_machine_id.size(); j++) {
					if (cannot_migrate[status.vr_machine_list[cur_server.vr_machine_id[j]].type]) {
						continue;
					}
					VrMachine& cur_vm = status.vr_machine_list[cur_server.vr_machine_id[j]];

					VrMachineType vm_type = input_variable.vr_machine_type[cur_vm.type];

					int rest_sources = 0;
					rest_sources = input_variable.server_type[cur_server.type].core + input_variable.server_type[cur_server.type].mem -
								cur_server.core[0] - cur_server.core[1] - cur_server.mem[0] - cur_server.mem[1];
					
					if (cur_server.vr_machine_id.size() != 1) {
						if (rest_sources >= input_variable.migrate_threshold) {
							continue;
						}
					}

					if (!status.migration_max) {
						break;
					}

					int cur_score[2] = {}, server_score[2] = {};
					if (vm_type.double_node) {
						for (int j = 0; j < 2; j++) {
							cur_server.core[j] += vm_type.core / 2;
							cur_server.mem[j] += vm_type.mem / 2;
						}
					} else {
						cur_server.core[cur_vm.node] += vm_type.core;
						cur_server.mem[cur_vm.node] += vm_type.mem;
					}
					ScoreServerWeight(cur_server, vm_type, cur_score, true);
					if (vm_type.double_node) {
						cur_server.core[0] -= vm_type.core / 2;
						cur_server.core[1] -= vm_type.core / 2;
						cur_server.mem[0] -= vm_type.mem / 2;
						cur_server.mem[1] -= vm_type.mem / 2;
					} else {
						cur_server.core[cur_vm.node] -= vm_type.core;
						cur_server.mem[cur_vm.node] -= vm_type.mem;
					}

					int find_server = -1, node = 0;
					int min_score = cur_score[cur_vm.node];
					for (int j = kSERVERFORDEL; j < status.server_list.size(); j++) {
						if (j == cur_vm.server_id) {
							if (vm_type.double_node == 1) {
								continue;
							} else {
								int tmp_node = cur_vm.node ^ 1;
								if (cur_score[tmp_node] < 1e8 && min_score > cur_score[tmp_node]) {
									min_score = cur_score[tmp_node];
									node = tmp_node;
									find_server = j;
									if (!SCOREMIGRATE) {
										break;
									}
								}
								continue;
							}
						}
						Server& target_server = status.server_list[j];
						ScoreServerWeight(target_server, vm_type, server_score, true);
						for (int k = 0; k < 2; k++) {
							if (server_score[k] < 1e8 && min_score > server_score[k]) {
								min_score = server_score[k];
								find_server = j;
								node = k;
								if (!SCOREMIGRATE) {
									break;
								}
							}
						}
						if (!SCOREMIGRATE && find_server != -1) {
							break;
						}
					}
					if (find_server != -1) {
						MoveVm(cur_vm, status.server_list[find_server], node, status);
						status.migration_max--;
					// 	memset(cannot_migrate, 0, sizeof cannot_migrate);
					// } else {
					// 	cannot_migrate[status.vr_machine_list[cur_server.vr_machine_id[j]].type] = 1;
					// 	VrMachineType a = input_variable.vr_machine_type[status.vr_machine_list[cur_server.vr_machine_id[j]].type];
					// 	for (int j = 0; j < input_variable.nMaxVrMachineType; j++) {
					// 		VrMachineType b = input_variable.vr_machine_type[j];
					// 		if (cannot_migrate[j] == 0 && a.core < b.core && a.mem < b.mem && a.double_node == b.double_node) {
					// 			cannot_migrate[j] = 1;
					// 		}
					// 	}
					}
				}
			}
		}
		if (DEBUG) {
			cout << "migration_times:" << status.migration_max << endl;
			// cout << "loop:" << loop << endl;
		}
	}
	static inline bool cmpVmId(pair<int, int> a, pair<int, int> b) {
		VrMachineType vm_type_a = input_variable.vr_machine_type[a.second];
		VrMachineType vm_type_b = input_variable.vr_machine_type[b.second];
		if (vm_type_a.double_node != vm_type_b.double_node) {
			return vm_type_a.double_node < vm_type_b.double_node;
		}
		return vm_type_a.core + vm_type_a.mem > vm_type_b.core + vm_type_b.mem;
	}
	void Migrate(Status& status, int day) {
		for (int times = 0; times < 2; times++) {
			vector<pair<int, int>> vm_sort_id;
			vm_sort_id.clear();
			for (int i = 0; i < status.vr_machine_list.size(); i++) {
				if (status.vr_machine_list[i].del) {
					continue;
				}
				vm_sort_id.push_back(make_pair(i, status.vr_machine_list[i].type));
			}
			if (SORTMIGRATE) {
				sort(vm_sort_id.begin(), vm_sort_id.end(), cmpVmId);
			}

			int cannot_migrate[kMaxVrMachineType] = {};
			for (int i = 0; i < vm_sort_id.size(); i++) {
				int vm_id = vm_sort_id[i].first;
				if (cannot_migrate[status.vr_machine_list[vm_id].type]) {
					continue;
				}
				VrMachineType vm_type = input_variable.vr_machine_type[status.vr_machine_list[vm_id].type];
				Server cur_server = status.server_list[status.vr_machine_list[vm_id].server_id];

				if (status.vr_machine_list[vm_id].server_id < kSERVERFORDEL) continue;

				int rest_sources = 0;
				rest_sources = input_variable.server_type[cur_server.type].core + input_variable.server_type[cur_server.type].mem -
							cur_server.core[0] - cur_server.core[1] - cur_server.mem[0] - cur_server.mem[1];
				
				if (cur_server.vr_machine_id.size() != 1) {
					if (rest_sources >= input_variable.migrate_threshold) {
						continue;
					}
				}

				if (!status.migration_max) {
					break;
				}

				int cur_score[2] = {}, server_score[2] = {};
				if (vm_type.double_node) {
					for (int j = 0; j < 2; j++) {
						cur_server.core[j] += vm_type.core / 2;
						cur_server.mem[j] += vm_type.mem / 2;
					}
				} else {
					cur_server.core[status.vr_machine_list[vm_id].node] += vm_type.core;
					cur_server.mem[status.vr_machine_list[vm_id].node] += vm_type.mem;
				}
				ScoreServerWeight(cur_server, vm_type, cur_score, true);
				if (vm_type.double_node) {
					cur_server.core[0] -= vm_type.core / 2;
					cur_server.core[1] -= vm_type.core / 2;
					cur_server.mem[0] -= vm_type.mem / 2;
					cur_server.mem[1] -= vm_type.mem / 2;
				} else {
					cur_server.core[status.vr_machine_list[vm_id].node] -= vm_type.core;
					cur_server.mem[status.vr_machine_list[vm_id].node] -= vm_type.mem;
				}

				int find_server = -1, node = 0;
				int min_score = cur_score[status.vr_machine_list[vm_id].node];
				for (int j = kSERVERFORDEL; j < status.server_list.size(); j++) {
					if (j == status.vr_machine_list[vm_id].server_id) {
						if (vm_type.double_node == 1) {
							continue;
						} else {
							int tmp_node = status.vr_machine_list[vm_id].node ^ 1;
							if (cur_score[tmp_node] < 1e8 && min_score > cur_score[tmp_node]) {
								min_score = cur_score[tmp_node];
								node = tmp_node;
								find_server = j;
								if (!SCOREMIGRATE) {
									break;
								}
							}
							continue;
						}
					}
					Server& target_server = status.server_list[j];
					ScoreServerWeight(target_server, vm_type, server_score, true);
					for (int k = 0; k < 2; k++) {
						if (server_score[k] < 1e8 && min_score > server_score[k]) {
							min_score = server_score[k];
							find_server = j;
							node = k;
							if (!SCOREMIGRATE) {
								break;
							}
						}
					}
					if (!SCOREMIGRATE && find_server != -1) {
						break;
					}
				}
				if (find_server != -1) {
					MoveVm(status.vr_machine_list[vm_id], status.server_list[find_server], node, status);
					status.migration_max--;
					// memset(cannot_migrate, 0, sizeof (cannot_migrate));
				} else {
					// cannot_migrate[status.vr_machine_list[vm_id].type] = 1;
					// VrMachineType a = input_variable.vr_machine_type[status.vr_machine_list[vm_id].type];
					// for (int j = 0; j < input_variable.nMaxVrMachineType; j++) {
					// 	VrMachineType b = input_variable.vr_machine_type[j];
					// 	if (cannot_migrate[j] == 0 && a.core <= b.core && a.mem <= b.mem && a.double_node == b.double_node) {
					// 		cannot_migrate[j] = 1;
					// 	}
					// }
				}
			}
		}
		if (DEBUG) {
			cout << "migration_times:" << status.migration_max << endl;
			// cout << "loop:" << loop << endl;
		}
	}
	void RandomMigrate(Status& status) {
		int loop = 0;
		while(status.migration_max) {
			if (loop == 100000) {
				break;
			}
			loop++;
			int i, j, k, node;
			if (status.server_list.size() - kSERVERFORDEL) {
				i = rand() % (status.server_list.size() - kSERVERFORDEL) + kSERVERFORDEL;
			} else {
				break;
			}
			
			if (status.server_list[i].vr_machine_id.size() == 0) {
				continue;
			}
			j = rand() % status.server_list[i].vr_machine_id.size();
			
			Server& cur_server = status.server_list[i];
			VrMachine& vm = status.vr_machine_list[cur_server.vr_machine_id[j]];
			VrMachineType vm_type = input_variable.vr_machine_type[vm.type];
			int cur_server_score[2] = {};
			if (vm_type.double_node) {
				for (int l = 0; l < 2; l++) {
					cur_server.core[l] += vm_type.core / 2;
					cur_server.mem[l] += vm_type.mem / 2;
				}
			} else {
				cur_server.core[vm.node] += vm_type.core;
				cur_server.mem[vm.node] += vm_type.mem;
			}
			ScoreServerWeight(cur_server, vm_type, cur_server_score, true);
			if (vm_type.double_node) {
				for (int l = 0; l < 2; l++) {
					cur_server.core[l] -= vm_type.core / 2;
					cur_server.mem[l] -= vm_type.mem / 2;
				}
			} else {
				cur_server.core[vm.node] -= vm_type.core;
				cur_server.mem[vm.node] -= vm_type.mem;
			}
			int find_server = -1, find_node = 0;
			int min_score = cur_server_score[vm.node];
			for (int loop_times = 0; loop_times < 10; loop_times++) {
				k = rand() % status.server_list.size();
				Server& target_server = status.server_list[k];
				int server_score[2] = {};
				if (vm_type.double_node) {
					node = 0;
				} else {
					node = vm.node ^ 1;
				}
				
				if (i == k) {
					if (node == vm.node || cur_server_score[node] == -1) {
						continue;
					}
					if (cur_server_score[node] != -1 && cur_server_score[node] >= min_score) {
						continue;
					}
					min_score = cur_server_score[node];
				} else {
					ScoreServerWeight(target_server, vm_type, server_score, true);
					if (server_score[node] == -1) {
						continue;
					}
					if (server_score[node] != -1 && server_score[node] >= min_score) {
						continue;
					}
					min_score = server_score[node];
				}
				find_server = target_server.id;
				find_node = node;
			}
			if (find_server != -1) {
				MoveVm(vm, status.server_list[find_server], find_node, status);
				status.migration_max--;
			}
		}

		if (DEBUG) {
			cout << "migration_times:" << status.migration_max << endl;
			cout << "loop:" << loop << endl;
		}
	}
	bool CheckSwap(Server& server_0, Server& server_1, VrMachine& vm_0, VrMachine& vm_1) {
		VrMachineType vm_type_0 = input_variable.vr_machine_type[vm_0.type];
		VrMachineType vm_type_1 = input_variable.vr_machine_type[vm_1.type];
		if (vm_type_0.double_node) {
			return min(server_0.core[0], server_0.core[1]) + vm_type_0.core / 2 - vm_type_1.core / 2 >= 0 && 
				   min(server_0.mem[0], server_0.mem[1]) + vm_type_0.mem / 2 - vm_type_1.mem / 2 >= 0 && 
				   min(server_1.core[0], server_1.core[1]) + vm_type_1.core / 2 - vm_type_0.core / 2 >= 0 && 
				   min(server_1.mem[0], server_1.mem[1]) + vm_type_1.mem / 2 - vm_type_0.mem / 2 >= 0;
		} else {
			return server_0.core[vm_0.node] + vm_type_0.core - vm_type_1.core >= 0 &&
				   server_0.mem[vm_0.node] + vm_type_0.mem - vm_type_1.mem >= 0 &&
				   server_1.core[vm_1.node] + vm_type_1.core - vm_type_0.core >= 0 && 
				   server_1.mem[vm_1.node] + vm_type_1.mem - vm_type_0.mem >= 0;
		}
	}
	bool ScoreSwap(Server& server_0, Server& server_1, VrMachine& vm_0, VrMachine& vm_1) {
		VrMachineType vm_type_0 = input_variable.vr_machine_type[vm_0.type];
		VrMachineType vm_type_1 = input_variable.vr_machine_type[vm_1.type];
		int core_ratio = input_variable.ratio_core, mem_ratio = input_variable.ratio_mem;
		if (vm_type_0.double_node) {
			// return core_ratio * (server_1.core[0] + server_1.core[1] + vm_type_1.core - vm_type_0.core) + 
			// 		   server_1.mem[0] + server_1.mem[1] + vm_type_1.mem - vm_type_0.mem;
			return min(core_ratio * (server_0.core[0] + server_0.core[1]) + server_0.mem[0] + server_0.mem[1],
					   core_ratio * (server_1.core[0] + server_1.core[1]) + server_1.mem[0] + server_1.mem[1]) -
				   min(core_ratio * (server_0.core[0] + server_0.core[1] + vm_type_0.core - vm_type_1.core) + 
					   mem_ratio * (server_0.mem[0] + server_0.mem[1] + vm_type_0.mem - vm_type_1.mem), 
					   core_ratio * (server_1.core[0] + server_1.core[1] + vm_type_1.core - vm_type_0.core) + 
					   mem_ratio * (server_1.mem[0] + server_1.mem[1] + vm_type_1.mem - vm_type_0.mem)) > 0;
		} else {
			// return core_ratio * (server_1.core[vm_1.node] + vm_type_1.core - vm_type_0.core) + 
			// 		   server_1.mem[vm_1.node] + vm_type_1.mem - vm_type_0.mem;
			return min(core_ratio * server_0.core[vm_0.node] + server_0.mem[vm_0.node],
					   core_ratio * server_1.core[vm_1.node] + server_1.mem[vm_1.node]) -
				   min(core_ratio * (server_0.core[vm_0.node] + vm_type_0.core - vm_type_1.core) + 
					   mem_ratio * (server_0.mem[vm_0.node] + vm_type_0.mem - vm_type_1.mem), 
					   core_ratio * (server_1.core[vm_1.node] + vm_type_1.core - vm_type_0.core) + 
					   mem_ratio * (server_1.mem[vm_1.node] + vm_type_1.mem - vm_type_0.mem)) > 0;
		}
	}
	static inline bool cmpSwapServerId(pair<int, Server> a, pair<int, Server> b) {
		int sources_a = input_variable.server_type[a.second.type].core + input_variable.server_type[a.second.type].mem -
							a.second.core[0] - a.second.core[1] - a.second.mem[0] - a.second.mem[1];
		int sources_b = input_variable.server_type[b.second.type].core + input_variable.server_type[b.second.type].mem -
							b.second.core[0] - b.second.core[1] - b.second.mem[0] - b.second.mem[1];
		int rest_sources_a = 0, rest_sources_b = 0;
		for (int i = 0; i < 2; i++) {
			rest_sources_a += a.second.core[i] + a.second.mem[i];
			rest_sources_b += b.second.core[i] + b.second.mem[i];
		}
		return rest_sources_a < rest_sources_b;
		// return sources_b > sources_a;
	}
	void SortSwapVm(Status& status, int day) {
		vector<pair<int, Server>> server_sort_id;
		server_sort_id.clear();
		for (int i = 0; i < status.server_list.size(); i++) {
			if (status.server_list[i].vr_machine_id.size() == 0) {
				continue;
			}
			server_sort_id.push_back(make_pair(i, status.server_list[i]));
		}

		sort(server_sort_id.begin(), server_sort_id.end(), cmpSwapServerId);
		int loop = 0;
		for (int i = 0; i < server_sort_id.size(); i++) {
			int server_id = server_sort_id[i].first;
			Server& cur_server = status.server_list[server_id];
			int rest_sources = 0;
			rest_sources = input_variable.server_type[cur_server.type].core + input_variable.server_type[cur_server.type].mem -
						   cur_server.core[0] - cur_server.core[1] - cur_server.mem[0] - cur_server.mem[1];
				
			// if (status.server_list[i].vr_machine_id.size() != 1) {
				if (rest_sources >= input_variable.swap_vm_threshold) {
					continue;
				}
			// }
			int cannot_migrate[kMaxVrMachineType] = {};
			for (int j = 0; j < cur_server.vr_machine_id.size(); j++) {
				if (cannot_migrate[status.vr_machine_list[cur_server.vr_machine_id[j]].type]) {
					continue;
				}
				bool flag = false;
				for (int k = kSERVERFORDEL; k < status.server_list.size(); k++) {
					if (server_id == k) {
						continue;
					}
					for (int l = 0; l < status.server_list[k].vr_machine_id.size(); l++) {
						loop++;
						if (loop == input_variable.swap_vm_loop_times) {
							if (DEBUG) {
								cout << "migration_times:" << status.migration_max << endl;
								cout << "loop:" << loop << endl;
							}
							return;
						}
						if (status.migration_max < 3) {
							if (DEBUG) {
								cout << "migration_times:" << status.migration_max << endl;
								cout << "loop:" << loop << endl;
							}
							return;
						}
						Server& server_0 = cur_server;
						VrMachine& vm_0 = status.vr_machine_list[server_0.vr_machine_id[j]];
						VrMachineType vm_type_0 = input_variable.vr_machine_type[vm_0.type];
						Server& server_1 = status.server_list[k];
						VrMachine& vm_1 = status.vr_machine_list[server_1.vr_machine_id[l]];
						VrMachineType vm_type_1 = input_variable.vr_machine_type[vm_1.type];

						if (vm_type_0.double_node != vm_type_1.double_node) {
							continue;
						}
						if (!CheckSwap(server_0, server_1, vm_0, vm_1)) {
							continue;
						}
						
						if (!ScoreSwap(server_0, server_1, vm_0, vm_1)) {
							continue;
						}
						flag = true;
						int tmp_check = PutCheck(vm_type_0, server_1);
						// if (tmp_check != -1) {
						// 	tmp_check %= 2;
						// 	if (vm_type_0.double_node) {
						// 		MoveVm(vm_0, server_1, 0, status);
						// 		MoveVm(vm_1, server_0, 0, status);
						// 	} else if (tmp_check == vm_1.node) {
						// 		bool node_0 = vm_0.node, node_1 = vm_1.node;
						// 		MoveVm(vm_0, server_1, node_1, status);
						// 		MoveVm(vm_1, server_0, node_0, status);
						// 	}
						// 	status.migration_max -= 2;
						// 	continue;
						// }
						// tmp_check = PutCheck(vm_type_1, server_0);
						// if (tmp_check != -1) {
						// 	tmp_check %= 2;
						// 	if (vm_type_0.double_node) {
						// 		MoveVm(vm_1, server_0, 0, status);
						// 		MoveVm(vm_0, server_1, 0, status);
						// 	} else if (tmp_check == vm_0.node) {
						// 		bool node_0 = vm_0.node, node_1 = vm_1.node;
						// 		MoveVm(vm_1, server_0, node_0, status);
						// 		MoveVm(vm_0, server_1, node_1, status);
						// 	}
						// 	status.migration_max -= 2;
						// 	continue;
						// }
						for (int p = 0; p < status.server_list.size(); p++) {
							if (p == i || p == k) {
								continue;
							}
							tmp_check = PutCheck(vm_type_0, status.server_list[p]);
							if (tmp_check != -1) {
								tmp_check %= 2;
								if (vm_type_0.double_node) {
									MoveVm(vm_0, status.server_list[p], 0, status);
									MoveVm(vm_1, server_0, 0, status);
									MoveVm(vm_0, server_1, 0, status);
								} else {
									bool node_0 = vm_0.node, node_1 = vm_1.node;
									MoveVm(vm_0, status.server_list[p], tmp_check, status);
									MoveVm(vm_1, server_0, node_0, status);
									MoveVm(vm_0, server_1, node_1, status);
								}
								status.migration_max -= 3;
								// memset(cannot_migrate, 0, sizeof(cannot_migrate));
								break;
							}
						}
					}
				}
				// if (!flag) {
				// 	cannot_migrate[status.vr_machine_list[status.server_list[i].vr_machine_id[j]].type] = 1;
				// }
			}
		}
		if (DEBUG) {
			cout << "migration_times:" << status.migration_max << endl;
			cout << "loop:" << loop << endl;
		}
		return;
	}
	void SwapVm(Status& status, int day) {
		int loop = 0;
		for (int i = kSERVERFORDEL; i < status.server_list.size(); i++) {
			int rest_sources = 0;
			rest_sources = input_variable.server_type[status.server_list[i].type].core + input_variable.server_type[status.server_list[i].type].mem -
						   status.server_list[i].core[0] - status.server_list[i].core[1] - status.server_list[i].mem[0] - status.server_list[i].mem[1];
				
			// if (status.server_list[i].vr_machine_id.size() != 1) {
				if (rest_sources >= input_variable.swap_vm_threshold) {
					continue;
				}
			// }
			int cannot_migrate[kMaxVrMachineType] = {};
			for (int j = 0; j < status.server_list[i].vr_machine_id.size(); j++) {
				if (cannot_migrate[status.vr_machine_list[status.server_list[i].vr_machine_id[j]].type]) {
					continue;
				}
				bool flag = false;
				for (int k = kSERVERFORDEL; k < status.server_list.size(); k++) {
					if (i == k) {
						continue;
					}
					for (int l = 0; l < status.server_list[k].vr_machine_id.size(); l++) {
						loop++;
						if (loop == input_variable.swap_vm_loop_times) {
							if (DEBUG) {
								cout << "migration_times:" << status.migration_max << endl;
								cout << "loop:" << loop << endl;
							}
							return;
						}
						if (status.migration_max < 3) {
							if (DEBUG) {
								cout << "migration_times:" << status.migration_max << endl;
								cout << "loop:" << loop << endl;
							}
							return;
						}
						Server& server_0 = status.server_list[i];
						VrMachine& vm_0 = status.vr_machine_list[server_0.vr_machine_id[j]];
						VrMachineType vm_type_0 = input_variable.vr_machine_type[vm_0.type];
						Server& server_1 = status.server_list[k];
						VrMachine& vm_1 = status.vr_machine_list[server_1.vr_machine_id[l]];
						VrMachineType vm_type_1 = input_variable.vr_machine_type[vm_1.type];

						if (vm_type_0.double_node != vm_type_1.double_node) {
							continue;
						}
						if (!CheckSwap(server_0, server_1, vm_0, vm_1)) {
							continue;
						}
						
						if (!ScoreSwap(server_0, server_1, vm_0, vm_1)) {
							continue;
						}
						flag = true;
						int tmp_check = PutCheck(vm_type_0, server_1);
						// if (tmp_check != -1) {
						// 	tmp_check %= 2;
						// 	if (vm_type_0.double_node) {
						// 		MoveVm(vm_0, server_1, 0, status);
						// 		MoveVm(vm_1, server_0, 0, status);
						// 	} else if (tmp_check == vm_1.node) {
						// 		bool node_0 = vm_0.node, node_1 = vm_1.node;
						// 		MoveVm(vm_0, server_1, node_1, status);
						// 		MoveVm(vm_1, server_0, node_0, status);
						// 	}
						// 	status.migration_max -= 2;
						// 	continue;
						// }
						// tmp_check = PutCheck(vm_type_1, server_0);
						// if (tmp_check != -1) {
						// 	tmp_check %= 2;
						// 	if (vm_type_0.double_node) {
						// 		MoveVm(vm_1, server_0, 0, status);
						// 		MoveVm(vm_0, server_1, 0, status);
						// 	} else if (tmp_check == vm_0.node) {
						// 		bool node_0 = vm_0.node, node_1 = vm_1.node;
						// 		MoveVm(vm_1, server_0, node_0, status);
						// 		MoveVm(vm_0, server_1, node_1, status);
						// 	}
						// 	status.migration_max -= 2;
						// 	continue;
						// }
						for (int p = 0; p < status.server_list.size(); p++) {
							if (p == i || p == k) {
								continue;
							}
							tmp_check = PutCheck(vm_type_0, status.server_list[p]);
							if (tmp_check != -1) {
								tmp_check %= 2;
								if (vm_type_0.double_node) {
									MoveVm(vm_0, status.server_list[p], 0, status);
									MoveVm(vm_1, server_0, 0, status);
									MoveVm(vm_0, server_1, 0, status);
								} else {
									bool node_0 = vm_0.node, node_1 = vm_1.node;
									MoveVm(vm_0, status.server_list[p], tmp_check, status);
									MoveVm(vm_1, server_0, node_0, status);
									MoveVm(vm_0, server_1, node_1, status);
								}
								status.migration_max -= 3;
								// memset(cannot_migrate, 0, sizeof(cannot_migrate));
								break;
							}
						}
					}
				}
				// if (!flag) {
				// 	cannot_migrate[status.vr_machine_list[status.server_list[i].vr_machine_id[j]].type] = 1;
				// }
			}
		}
		if (DEBUG) {
			cout << "migration_times:" << status.migration_max << endl;
			cout << "loop:" << loop << endl;
		}
		return;
	}
	static inline bool ServerCmp(Server a, Server b) {
		if (a.purchase_day != b.purchase_day) {
			return a.purchase_day < b.purchase_day;
		}
		return a.type < b.type;
	}
	void UpdateServerId(Status status, Decision& today_decision) {
		stable_sort(status.server_list.begin(), status.server_list.end(), ServerCmp);
		int index[100000 + 5] = {};
		for (int i = 0; i < status.server_list.size(); i++) {
			index[status.server_list[i].id] = i;
		}
		for (int k = 0; k < today_decision.deploy.size(); k++) {
			today_decision.deploy[k].server_id = index[today_decision.deploy[k].server_id];
		}
		for (int k = 0; k < today_decision.migration.size(); k++) {
			int tmp = get<1>(today_decision.migration[k]);
			get<1>(today_decision.migration[k]) = index[tmp];
		}
	}
	static inline bool cmpDeployIdx(Deploy a, Deploy b) {
		return a.idx < b.idx;
	}
	static inline bool cmpRequestNodePlus(Request a, Request b) {
		VrMachineType a_type = input_variable.vr_machine_type[input_variable.vr_machine_type2int[a.type]];
		VrMachineType b_type = input_variable.vr_machine_type[input_variable.vr_machine_type2int[b.type]];
		// if (a.del_day != b.del_day) {
		// 	return a.del_day < b.del_day;
		// }
		if (a_type.double_node != b_type.double_node) {
			return a_type.double_node > b_type.double_node;
		}
		return a_type.mem + a_type.core > b_type.mem + b_type.core;
	}
	int CalMostUseServerType() {
		int cnt[kMaxServerType] = {};
		for (int j = 0; j < input_variable.nMaxVrMachineType; j++) {
			cnt[input_variable.target_server_type[j]]++;
		}
		int ret = 0;
		for (int i = 0; i < input_variable.nMaxServerType; i++) {
			if (cnt[ret] < cnt[i]) {
				ret = i;
			}
		}
		return ret;
	}
	void MigrateForDel(Status& status, int day) {
		int cnt = 0;
		for (int i = 0; i < status.vr_machine_list.size(); i++) {
			if (status.vr_machine_list[i].del_day != day) {
				continue;
			}
			VrMachineType vm_type = input_variable.vr_machine_type[status.vr_machine_list[i].type];
			Server cur_server = status.server_list[status.vr_machine_list[i].server_id];

			if (!status.migration_max) {
				break;
			}

			int server_score[2] = {};
			int find_server = -1, node = 0;
			int min_score = 1e9;
			for (int j = 0; j < kSERVERFORDEL; j++) {
				Server& target_server = status.server_list[j];
				ScoreServerWeight(target_server, vm_type, server_score, true);
				for (int k = 0; k < 2; k++) {
					if (server_score[k] < 1e8 && min_score > server_score[k]) {
						min_score = server_score[k];
						find_server = j;
						node = k;
					}
				}
			}
			if (find_server != -1) {
				MoveVm(status.vr_machine_list[i], status.server_list[find_server], node, status);
				status.migration_max--;
			} else {
				cnt++;
			}
		}
		cout << "can't migrate:" << cnt << endl;
	}
	void Solve(int day) {
		if (DEBUG) {
			cout << "day: " << day << endl;
		}
		memset(pre_status.purchase_decision, 0, sizeof(pre_status.purchase_decision));
		today_decision.clear();
		cur_status = pre_status;
		cur_status.migration_max = 0;
		cur_status.requests.clear();
		vector<Request> tmp_requests;
		if (!SORTREQUEST) {
			cur_status.requests = day_request[day].requests;
		}

		int cnt = 0;
		for (int i = 0; i < cur_status.vr_machine_list.size(); i++) {
			if (!cur_status.vr_machine_list[i].del) {
				cnt++;
			}
		}
		cur_status.migration_max = 3 * cnt / 100;

		if (SORTREQUEST) {
			tmp_requests = day_request[day].requests;
			for (int i = 0; i < tmp_requests.size(); i++) {
				if (tmp_requests[i].op == "del") {
					if (cur_status.vr_machine_id2int.count(tmp_requests[i].id)) {
						cur_status.vr_machine_list[cur_status.vr_machine_id2int[tmp_requests[i].id]].del_day = day;
					}
				}
			}

			sort(tmp_requests.begin(), tmp_requests.end(), cmpRequestNodePlus);
			for (int i = 0; i < tmp_requests.size(); i++) {
				if (tmp_requests[i].op == "add") {
					cur_status.requests.push_back(tmp_requests[i]);
				}
			}
			for (int i = 0; i < tmp_requests.size(); i++) {
				if (tmp_requests[i].op == "del") {
					cur_status.requests.push_back(tmp_requests[i]);
				}
			}
		}		

		if (BACKOFF) {
			pre_status = cur_status;
			for (int j = 0; j < pre_status.requests.size(); j++) {
				Request& tmp = pre_status.requests[j];
				if (tmp.op == "add") {
					AddVMScored(tmp, pre_status, day, 1);
				} else {
					DelVM(tmp, pre_status);
				}
			}
			today_decision.deploy.clear();
			for (int i = 0; i < input_variable.nMaxServerType; i++) {
				for (int j = 0; j < pre_status.purchase_decision[i]; j++) {
					PurchaseServer(i, day, cur_status);
				}
			}
		}

		if (day == 0) {
			for (int i = 0; i < kSERVERFORDEL; i++) {
				PurchaseServer(input_variable.one_server_type, 0, cur_status);
			}
		}

		// MigrateForDel(cur_status, day);
		// SortServerMigrate(cur_status, day);
		Migrate(cur_status, day);
		// RandomMigrate(cur_status);
		// int tmp_migration = 0;
		// while (cur_status.migration_max != tmp_migration) {
		for (int i = 0; i < 1; i++) {
      		// tmp_migration = cur_status.migration_max;
			SwapVm(cur_status, day);
			// SortSwapVm(cur_status, day);
		}
		
		// RandomSwapVm(cur_status, day);
		// Migrate(cur_status, day);

		int mem_all = 0, core_all = 0, pre_times = 100;
		for (int k = 0; k < pre_times && k < cur_status.requests.size(); k++) {
			if (cur_status.requests[k].op == "del") {
				continue;
			}
			mem_all += input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[k].type]].mem;
			core_all += input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[k].type]].core;
		}
		// cout << 1.0 * core_all / mem_all << endl;

		for (int j = 0; j < cur_status.requests.size(); j++) {
			double next_mem_core_ratio = 1.0 * core_all / mem_all;
			Request& tmp = cur_status.requests[j];
			if (tmp.op == "add") {
				AddVMScored(tmp, cur_status, day, next_mem_core_ratio);
				mem_all -= input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[j].type]].mem;
				core_all -= input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[j].type]].core;
			} else {
				DelVM(tmp, cur_status);
			}
			if (j + pre_times < cur_status.requests.size() && cur_status.requests[j + pre_times].op == "add") {
				mem_all += input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[j + pre_times].type]].mem;
				core_all += input_variable.vr_machine_type[input_variable.vr_machine_type2int[cur_status.requests[j + pre_times].type]].core;
			}
		}
		CalPurchase(cur_status);
		UpdateServerId(cur_status, today_decision);
		if (SORTREQUEST) {
			sort(today_decision.deploy.begin(), today_decision.deploy.end(), cmpDeployIdx);
		}
		decision[day] = today_decision;
		for (int j = 0; j < cur_status.server_list.size(); j++) {
            if (cur_status.server_list[j].vr_machine_id.size()) {
                cur_status.score += input_variable.server_type[cur_status.server_list[j].type].energe_cost;
            }
        }
		pre_status = cur_status;
	}
	void Output(int day) {
		Decision dec = decision[day];
		cout << "(purchase, " << dec.purchase.size() << ")" << endl;
		for (int j = 0; j < dec.purchase.size(); j++) {
			cout << "(" << dec.purchase[j].first << ", " << dec.purchase[j].second << ")" << endl;
		}

		cout << "(migration, " << dec.migration.size() << ")" << endl;
		for (int j = 0; j < dec.migration.size(); j++) {
			if (get<2>(dec.migration[j]) == '0') {
				cout << "(" << get<0>(dec.migration[j]) << ", " << get<1>(dec.migration[j]) << ")" << endl;
			} else {
				cout << "(" << get<0>(dec.migration[j]) << ", " << get<1>(dec.migration[j]) << ", " << get<2>(dec.migration[j]) << ")" << endl;
			}
		}

		for (int j = 0; j < dec.deploy.size(); j++) {
			if (dec.deploy[j].node == -1) {
				cout << "(" << dec.deploy[j].server_id << ")" << endl;
			} else if (dec.deploy[j].node == 0) {
				cout << "(" << dec.deploy[j].server_id << ", A)" << endl;
			} else {
				cout << "(" << dec.deploy[j].server_id << ", B)" << endl;
			}
		}
	}
} process, pretest;

void SearchtTargetServer(int day, bool pretestpara) {
	LL hardware_cost_all = 0, energe_cost_all = 0;
	for (int i = 0; i < input_variable.nMaxServerType; i++) {
		hardware_cost_all += input_variable.server_type[i].hardware_cost;
		energe_cost_all += input_variable.server_type[i].energe_cost;
	}
	input_variable.one_server_type = -1;
    double cost = 1e9;
    vector<int> server_waitlist;
	for (int i = 0; i < input_variable.nMaxServerType; i++) {
        bool flag = true;
        for (int j = 0; j < input_variable.nMaxVrMachineType; j++) {
            if (input_variable.vr_machine_type[j].double_node) {
                if (input_variable.server_type[i].core < input_variable.vr_machine_type[j].core || input_variable.server_type[i].mem < input_variable.vr_machine_type[j].mem) {
                    flag = false;
                    break;
                }
            } else if (input_variable.server_type[i].core / 2 < input_variable.vr_machine_type[j].core || input_variable.server_type[i].mem / 2 < input_variable.vr_machine_type[j].mem) {
                flag = false;
                break;
            }
        }
		double tmp_cost = input_variable.server_type[i].hardware_cost;
        if (flag && cost > tmp_cost) {
            cost = tmp_cost;
            input_variable.one_server_type = i;
        }
		if (flag)
        server_waitlist.push_back(i);
	}
	for (int j = 0; j < input_variable.nMaxVrMachineType; j++) {
		cost = 1e9;
		for (int i = 0; i < input_variable.nMaxServerType; i++) {
			if (input_variable.vr_machine_type[j].double_node) {
                if (input_variable.server_type[i].core < input_variable.vr_machine_type[j].core * input_variable.target_server_ratio ||
					input_variable.server_type[i].mem < input_variable.vr_machine_type[j].mem * input_variable.target_server_ratio) {
                    continue;
                }
            } else if (input_variable.server_type[i].core / 2 < input_variable.vr_machine_type[j].core * input_variable.target_server_ratio ||
						input_variable.server_type[i].mem / 2 < input_variable.vr_machine_type[j].mem * input_variable.target_server_ratio) {
                continue;
            }
			double tmp_cost = input_variable.server_type[i].hardware_cost +
						  	  input_variable.server_type[i].energe_cost * (input_variable.nMaxDay - day);
			if (hardware_cost_all < 333 * energe_cost_all) {
				tmp_cost = input_variable.server_type[i].energe_cost;
			}
			if (pretestpara) {
				tmp_cost = input_variable.server_type[i].hardware_cost +
						  	  input_variable.server_type[i].energe_cost * 1000;
			}
			if (cost > tmp_cost) {
				cost = tmp_cost;
				input_variable.target_server_type[j] = i;
			}
		}
		if (cost > 1e8)
		for (int i = 0; i < input_variable.nMaxServerType; i++) {
			if (input_variable.vr_machine_type[j].double_node) {
                if (input_variable.server_type[i].core < input_variable.vr_machine_type[j].core ||
					input_variable.server_type[i].mem < input_variable.vr_machine_type[j].mem) {
                    continue;
                }
            } else if (input_variable.server_type[i].core / 2 < input_variable.vr_machine_type[j].core ||
						input_variable.server_type[i].mem / 2 < input_variable.vr_machine_type[j].mem) {
                continue;
            }
			double tmp_cost = input_variable.server_type[i].hardware_cost +
						  	  input_variable.server_type[i].energe_cost * (input_variable.nMaxDay - day);
			if (hardware_cost_all < 333 * energe_cost_all) {
				tmp_cost = input_variable.server_type[i].energe_cost;
			}
			if (pretestpara) {
				tmp_cost = input_variable.server_type[i].hardware_cost +
						  	  input_variable.server_type[i].energe_cost * 1000;
			}
			if (cost > tmp_cost) {
				cost = tmp_cost;
				input_variable.target_server_type[j] = i;
			}
		}
	}
}

void Interactive() {
	if (DEBUG) {
		freopen("training-2.txt", "r", stdin);
	}
	ios::sync_with_stdio(false);
	cin.tie(0);
	cin >> input_variable.nMaxServerType;
	for (int i = 0; i < input_variable.nMaxServerType; i++) {
		string type = input_variable.server_type[i].Input(i);
		input_variable.server_type2int[type] = i;
	}
	cin >> input_variable.nMaxVrMachineType;
	for (int i = 0; i < input_variable.nMaxVrMachineType; i++) {
		string type = input_variable.vr_machine_type[i].Input(i);
		input_variable.vr_machine_type2int[type] = i;
	}

	int tmpa = -1, tmpb = -1;
	LL min_score = 1e15;
	input_variable.swap_vm_loop_times = 4000;
	for (int a = 0; a <= 3; a++) {
		input_variable.target_server_ratio = 1 + a * 0.1;
		for (int b = 1; b <= 3; b++) {
			input_variable.ratio_core = b;
			pretest.pre_status.clear();
			input_variable.nMaxDay = 1;
			for (int i = 0; i < input_variable.nMaxDay; i++) {
				pretest.day_request[i].requests.clear();
			}
			int vm_id_cnt = 0;
			for (int i = 0; i < input_variable.nMaxDay; i++) {
				SearchtTargetServer(i, true);
				input_variable.nMaxRequest = input_variable.nMaxVrMachineType * 5;
				for (int j = 0; j < input_variable.nMaxRequest; j++) {
					Request tmp;
					tmp.day = i;
					tmp.del_day = -1;
					tmp.idx = j;
					tmp.op = "add";
					tmp.type = input_variable.vr_machine_type[j % input_variable.nMaxVrMachineType].type;
					// tmp.type = input_variable.vr_machine_type[rand() % input_variable.nMaxVrMachineType].type;
					tmp.id = vm_id_cnt++;
					pretest.day_request[i].requests.push_back(tmp);
				}
				pretest.Solve(i);
			}
			if (min_score > pretest.cur_status.score) {
				min_score = pretest.cur_status.score;
				tmpa = a;
				tmpb = b;
			}
			cout << min_score << endl;
		}
	}
	input_variable.target_server_ratio = 1 + tmpa * 0.1;
	input_variable.ratio_core = 1;
	
	input_variable.swap_vm_loop_times = 400000;
	process.pre_status.clear();
	cin >> input_variable.nMaxDay >> input_variable.nMaxPreDay;
	// if (input_variable.nMaxPreDay > 30)
	input_variable.nMaxPreDay = 1;
	for (int i = 0; i < input_variable.nMaxPreDay; i++) {
		cin >> input_variable.nMaxRequest;
		for (int j = 0; j < input_variable.nMaxRequest; j++) {
			Request tmp;
			tmp.day = i;
			tmp.del_day = 2000;
			tmp.idx = j;
			tmp.Input();
			if (tmp.op == "del") {
				int x = input_variable.request_id2day_idx[tmp.id].first;
				int y = input_variable.request_id2day_idx[tmp.id].second;
				process.day_request[x].requests[y].del_day = i;
			} else {
				input_variable.request_id2day_idx[tmp.id] = make_pair(i, j);
			}
			process.day_request[i].requests.push_back(tmp);
		}
	}

	for (int i = 0; i < input_variable.nMaxDay; i++) {
		SearchtTargetServer(i, false);
		process.Solve(i);
		if (!DEBUG) {
			process.Output(i);
		}
		fflush(stdout);
		if (i + input_variable.nMaxPreDay < input_variable.nMaxDay) {
			cin >> input_variable.nMaxRequest;
			for (int j = 0; j < input_variable.nMaxRequest; j++) {
				Request tmp;
				tmp.day = i + input_variable.nMaxPreDay;
				tmp.del_day = 2000;
				tmp.idx = j;
				tmp.Input();
				if (tmp.op == "del") {
					int x = input_variable.request_id2day_idx[tmp.id].first;
					int y = input_variable.request_id2day_idx[tmp.id].second;
					process.day_request[x].requests[y].del_day = i + input_variable.nMaxPreDay;
				} else {
					input_variable.request_id2day_idx[tmp.id] = make_pair(i + input_variable.nMaxPreDay, j);
				}
				process.day_request[i + input_variable.nMaxPreDay].requests.push_back(tmp);
			}
		}
	}
	if (DEBUG) {
		cout << process.cur_status.score << endl;
		cout << input_variable.target_server_ratio << " " << input_variable.ratio_core << endl;
	}
}
int main() {
	srand((unsigned)time(NULL));
	Interactive();
	return 0;
}