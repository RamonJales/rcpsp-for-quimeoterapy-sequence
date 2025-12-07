#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <tuple>
#include <random>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <climits>
#include <chrono>
#include <filesystem>
#include <iomanip>

using namespace std;
namespace fs = std::filesystem;

/**
 * @brief Representa uma tarefa (job) no projeto RCPSP.
 * * Esta estrutura armazena todas as informações estáticas (grafo, recursos)
 * e dinâmicas (tempos calculados, estados) de uma única atividade.
 */
struct node
{
    int id;

    // --- Topologia do Grafo ---
    vector<int> predecessors;
    vector<int> successors;
    vector<int> renewable_resource_requirements;

    int start_time;
    int finish_time;
    int duration_time;

    // for calculate critic path (CPM)
    int earliest_start;
    int latest_start;
    int earliest_finish;
    int latest_finish;

    // --- Atributos do Algoritmo Genético ---
    double priority_value;
    double selection_probability;

    bool started;
    bool finished;
    bool scheduled;

    /**
     * @brief Construtor padrão.
     * Inicializa os IDs e tempos com -1 (indicando "não definido") e flags como
     * false.
     */
    node() {
        id = -1;

        start_time = -1;
        finish_time = -1;
        duration_time = 0;

        earliest_start = -1;
        latest_start = -1;
        earliest_finish = -1;
        latest_finish = -1;

        priority_value = 0.0;
        selection_probability = 0.0;

        started = false;
        finished = false;
        scheduled = false;
    }
};

/**
 * @brief Representa um Indivíduo (Cromossomo) na população do Algoritmo
 * Genético.
 * * Um indivíduo carrega uma permutação de tarefas (genótipo) e o agendamento
 * resultante (fenótipo).
 */
struct individual 
{
    /// O valor da função objetivo (Makespan).
    double fitness;

    vector<int> activity_list;

    /// Mapa {ID da Tarefa -> Tempo de Início}. Representa quando cada tarefa
    /// começa nesta solução.
    map<int, int> start_times;
    map<int, int> finish_times;

    /**
     * @brief Construtor padrão.
     * Inicializa o fitness com infinito positivo (pior caso para minimização).
     */
    individual() { fitness = numeric_limits<double>::infinity(); }

    /**
     * @brief Verifica se a activity_list respeita as restrições de precedência
     * topológica.
     * * Garante que nenhum sucessor apareça antes de seu predecessor na lista de
     * prioridades. Isso é crucial para que o SGS Serial funcione corretamente.
     * * @param all_nodes Referência ao vetor principal contendo os dados de todos
     * os nós do projeto.
     * @return true Se a lista for viável (topologicamente ordenada).
     * @return false Se houver violação de precedência.
     */
    bool check_precedence_feasibility(vector<node> &all_nodes) {
        vector<int> position_in_list(all_nodes.size(), -1);

        for (size_t i = 0; i < activity_list.size(); ++i) {
            position_in_list[activity_list[i]] = i;
        }

        for (int node_id : activity_list) {
            int current_position = position_in_list[node_id];

            const node &current_node = all_nodes[node_id];

            for (int predecessor_id : current_node.predecessors) {
                if (position_in_list[predecessor_id] != -1) {
                    if (position_in_list[predecessor_id] > current_position) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    void restore_precedence_of_activity_list() {}
};

struct project {
    /// @brief gerador de número aleatório (Mersene Twistter)
    mt19937 rng;

    /// @brief dados do problema
    int number_of_jobs;
    int number_of_nondummy_jobs;
    int number_of_renewable_resources;
    int horizon;
    int cpm_lower_bound;

    vector<int> renewable_resource_availability;
    vector<individual> population;

    vector<node> nodes;

    project() {
        random_device rd;
        rng = mt19937(rd());
        cpm_lower_bound = 0;
    }

    void clear() {
        nodes.clear();
        renewable_resource_availability.clear();
        population.clear();
        number_of_jobs = 0;
        horizon = 0;
        cpm_lower_bound = 0;
    }

    /**
     * @brief Calcula o Caminho Crítico (CPM).
     * Preenche ES, EF, LS, LF de todos os nós.
     * Necessário para o método de amostragem da população inicial.
     */
    void forward_backward_scheduling() {
        if (nodes.empty())
            return;

        calculate_forward_pass();
        calculate_backward_pass();

        if (!nodes.empty()) {
            cpm_lower_bound = nodes[number_of_jobs - 1].earliest_finish;
        }
    }

    /**
     * @brief Lê e analisa um arquivo de instância (formato PSPLIB .sm).
     * * Esta função carrega o arquivo inteiro na memória, identifica seções por
     * palavras-chave
     * ("projects", "PRECEDENCE RELATIONS", etc.) e preenche os vetores de nós e
     * recursos.
     * * @param instance_filepath Caminho do diretório onde o arquivo está.
     * @param instance_name Nome do arquivo (ex: "j1201_1.sm").
     */
    void read_project(string full_path) {
        clear();
        vector<string> file_lines = load_file_in_memory(full_path);
        if (file_lines.empty()) return;

        for (size_t line_index = 0; line_index < file_lines.size(); ++line_index) {
            string current_line = file_lines[line_index];

            if (current_line.find("jobs (incl. supersource/sink )") != string::npos) {
                stringstream ss_jobs(current_line);
                string temp; int val;
                while (ss_jobs >> temp) { if (stringstream(temp) >> val) number_of_jobs = val; }
                number_of_nondummy_jobs = number_of_jobs - 2;

                if (line_index + 1 < file_lines.size()) {
                    stringstream ss_hor(file_lines[line_index + 1]);
                    while (ss_hor >> temp) { if (stringstream(temp) >> val) horizon = val; }
                }
                if (line_index + 2 < file_lines.size()) {
                    stringstream ss_res(file_lines[line_index + 2]);
                    vector<int> numbers;
                    while (ss_res >> temp) { if (stringstream(temp) >> val) numbers.push_back(val); }
                    if (!numbers.empty()) number_of_renewable_resources = numbers.back();
                }
                nodes.resize(number_of_jobs);
                for (int i = 0; i < number_of_jobs; ++i) nodes[i].id = i;
            }
            else if (current_line.find("PRECEDENCE RELATIONS:") != string::npos) {
                size_t starting_line_index = line_index + 2;
                for (int i = 0; i < number_of_jobs; ++i) {
                    if (starting_line_index + i >= file_lines.size()) break;
                    stringstream ss(file_lines[starting_line_index + i]);
                    int job_idx, mode, num_successors;
                    ss >> job_idx >> mode >> num_successors;
                    for (int j = 0; j < num_successors; ++j) {
                        int succ_id; ss >> succ_id;
                        nodes[i].successors.push_back(succ_id - 1);
                    }
                }
                for (int i = 0; i < number_of_jobs; ++i) {
                    for (int succ_id : nodes[i].successors) nodes[succ_id].predecessors.push_back(i);
                }
            }
            else if (current_line.find("REQUESTS/DURATIONS:") != string::npos) {
                size_t starting_line_index = line_index + 2;
                for (int i = 0; i < number_of_jobs; ++i) {
                    if (starting_line_index + i >= file_lines.size()) break;
                    stringstream ss(file_lines[starting_line_index + i]);
                    int job_idx, mode, duration;
                    ss >> job_idx >> mode >> duration;
                    nodes[i].duration_time = duration;
                    int req;
                    for (int k = 0; k < number_of_renewable_resources; ++k) {
                        if (ss >> req) nodes[i].renewable_resource_requirements.push_back(req);
                    }
                }
            }
            else if (current_line.find("RESOURCEAVAILABILITIES:") != string::npos) {
                size_t starting_line_index = line_index + 2;
                if (starting_line_index < file_lines.size()) {
                     stringstream ss(file_lines[starting_line_index]);
                     int avail;
                     while (ss >> avail) renewable_resource_availability.push_back(avail);
                     if (renewable_resource_availability.size() > (size_t)number_of_renewable_resources)
                         renewable_resource_availability.resize(number_of_renewable_resources);
                }
            }
        }
    }

    /**
     * @brief Imprime os detalhes do projeto carregado no console.
     * Útil para debug e validação do parser.
     */
    void print_project() {
        if (nodes.empty()) {
            cout << "O projeto está vazio ou não foi carregado corretamente." << endl;
            return;
        }

        cout << "==========================================================" << endl;
        cout << "                PROJECT SUMMARY                           " << endl;
        cout << "==========================================================" << endl;
        cout << "Total Jobs (incl. dummy): " << number_of_jobs << endl;
        cout << "Real Jobs:                " << number_of_nondummy_jobs << endl;
        cout << "Horizon:                  " << horizon << endl;
        cout << "Renewable Resources:      " << number_of_renewable_resources << endl;
        
        cout << "Resource Availabilities:  [ ";
        for (int avail : renewable_resource_availability) {
            cout << avail << " ";
        }
        cout << "]" << endl << endl;

        cout << "----------------------------------------------------------" << endl;
        cout << " JOB DETAILS (ID | Dur | Res | Succs | Preds)" << endl;
        cout << "----------------------------------------------------------" << endl;

        for (const auto& node : nodes) {
            cout << "Job " << node.id + 1;
            cout << "\t| Dur: " << node.duration_time;
            
            // Requisitos de Recursos
            cout << "\t| Res: [";
            for (size_t k = 0; k < node.renewable_resource_requirements.size(); ++k) {
                cout << node.renewable_resource_requirements[k] << (k < node.renewable_resource_requirements.size() - 1 ? " " : "");
            }
            cout << "]";

            cout << "\t| Succ: {";
            for (size_t k = 0; k < node.successors.size(); ++k) {
                cout << node.successors[k] + 1 << (k < node.successors.size() - 1 ? "," : "");
            }
            cout << "}";
            
            // Predecessores (Opcional, para verificar lógica inversa)
            /*
            cout << "\t| Pred: {";
            for (size_t k = 0; k < node.predecessors.size(); ++k) {
                cout << node.predecessors[k] + 1 << (k < node.predecessors.size() - 1 ? "," : "");
            }
            cout << "}";
            */
            
            cout << endl;
        }
        cout << "==========================================================" << endl;
    }

    /**
     * @brief Create Initial Population: Cria a população inicial de um determinado tamanho.
     */
    vector<individual> create_initial_population(int population_size) {

        forward_backward_scheduling();

        vector<individual> population;

        individual dummy;

        vector<int> nodes_id_sorted_by_min_lft;
        for (auto &nd : nodes)
            nodes_id_sorted_by_min_lft.push_back(nd.id);

        sort(nodes_id_sorted_by_min_lft.begin(),
                nodes_id_sorted_by_min_lft.end(),
                [&](int a, int b){
                    return nodes[a].latest_finish < nodes[b].latest_finish;
                });

        dummy.activity_list = nodes_id_sorted_by_min_lft;
        population.push_back(dummy);


        for (int p = 1; p < population_size; p++) {

            individual new_dummy;

            vector<int> unselected_nodes;
            for (auto &nd : nodes) {
                unselected_nodes.push_back(nd.id);
            }

            vector<int> selected_nodes;

            int starting_node = unselected_nodes[0];
            selected_nodes.push_back(starting_node);
            unselected_nodes.erase(unselected_nodes.begin());

            while (!unselected_nodes.empty()) {

                vector<int> possibles;

                // === encontrar possíveis ===
                for (int node_id : unselected_nodes) {

                    bool all_in_selected = true;
                    for (int predecessor : nodes[node_id].predecessors) {

                        if (std::find(selected_nodes.begin(),
                                    selected_nodes.end(),
                                    predecessor) == selected_nodes.end()) {

                            all_in_selected = false;
                            break;
                        }
                    }

                    if (all_in_selected)
                        possibles.push_back(node_id);
                }

                // === calcular probabilidades ===
                int max_lft = -1;
                for (int node_id : possibles) {
                    max_lft = max(max_lft, nodes[node_id].latest_finish);
                }

                int total = 0;
                for (int node_id : possibles) {
                    total += max_lft - nodes[node_id].latest_finish + 1;
                }
                    
                for (int node_id : possibles) {
                    nodes[node_id].selection_probability =
                        double(max_lft - nodes[node_id].latest_finish + 1) / total;
                }
                    
                double total_prob = 0.0;
                for (int node_id : possibles) {
                    total_prob += nodes[node_id].selection_probability;
                }
                    
                uniform_real_distribution<double> dist(0.0, total_prob);
                double r = dist(rng);

                double acc = 0.0;
                int selected_node = possibles.back();

                for (int nid : possibles) {
                    acc += nodes[nid].selection_probability;
                    if (r < acc) {
                        selected_node = nid;
                        break;
                    }
                }

                selected_nodes.push_back(selected_node);
                unselected_nodes.erase(
                    std::remove(unselected_nodes.begin(),
                                unselected_nodes.end(),
                                selected_node),
                    unselected_nodes.end()
                );
            }

            new_dummy.activity_list = selected_nodes;
            population.push_back(new_dummy);
        }

        return population;
    }

    /**
     * @brief Crossover: Realiza os casamentos para gerar novas soluções para uma determinada população.
     */
    vector<individual> crossover(vector<individual> population) {
        random_device rd;
        mt19937 rng(rd());
        shuffle(population.begin(), population.end(), rng);

        int half = population.size() / 2;
        vector<individual> mothers(population.begin(), population.begin() + half);
        vector<individual> fathers(population.begin() + half, population.end());

        vector<individual> offspring;
        size_t n = min(mothers.size(), fathers.size());

        int max_q = number_of_nondummy_jobs - 1;
        uniform_int_distribution<int> dist(1, max_q);
        int q = dist(rng);

        for (size_t i = 0; i < n; i++) {
            auto& mother = mothers[i];
            auto& father = fathers[i];

            individual daughter;

            vector<int> mother_input_d(mother.activity_list.begin(), 
                mother.activity_list.begin() + q);

            vector<int> father_input_d;
            for (int activity : father.activity_list) {
                if (find(mother_input_d.begin(), mother_input_d.end(), activity) == mother_input_d.end()) {
                    father_input_d.push_back(activity);
                }
            }

            daughter.activity_list = mother_input_d;
            daughter.activity_list.insert(
                daughter.activity_list.end(),
                father_input_d.begin(), 
                father_input_d.end()
            );
            offspring.push_back(daughter);

            individual son;
 
            vector<int> father_input_s(father.activity_list.begin(),
                father.activity_list.begin() + q);

            vector<int> mother_input_s;
            for (int activity : mother.activity_list) {
                if (find(father_input_s.begin(), father_input_s.end(), activity) == father_input_s.end()) {
                    mother_input_s.push_back(activity);
                }
            }

            son.activity_list = father_input_s;
            son.activity_list.insert(
                son.activity_list.end(),
                mother_input_s.begin(), 
                mother_input_s.end()
            );
            offspring.push_back(son);
        }

        return offspring;
    }

    vector<individual> mutate(vector<individual> offsprings, double mutation_probability) {
        random_device rd;
        mt19937 rng(rd());

        for (auto &individual : offsprings) {
            for (size_t i = 0; i + 1 < individual.activity_list.size(); i++) {
                uniform_real_distribution<double> dist(0.0, 1.0);
                double r = dist(rng);

                if (r < mutation_probability) {
                    vector<int> stored_current_activity_list = individual.activity_list;
                    swap(individual.activity_list[i], individual.activity_list[i + 1]);
                    if (!individual.check_precedence_feasibility(nodes)) {
                        individual.activity_list = stored_current_activity_list;
                    }      
                }
            }
        }

        return offsprings;
    }

    /**
     * @brief Rank and Reduce: Avalia os filhos, une com a população atual, ordena e corta.
     * Agora aceita ponteiro para função membro (project::*sgs).
     */
    pair<vector<individual>, individual> rank_and_reduce(
        vector<individual> current_population, 
        vector<individual> offsprings, 
        individual incumbent, 
        void (project::*sgs)(individual &)) 
    { 
        // 1. Avaliar os filhos usando o SGS passado
        for (individual &indiv : offsprings) {
            (this->*sgs)(indiv);
        }

        // 2. Unir populações
        vector<individual> new_population = current_population;
        new_population.insert(new_population.end(), offsprings.begin(), offsprings.end());

        // 3. Ordenar (Menor makespan é melhor)
        sort(new_population.begin(), new_population.end(), 
            [](const individual& a, const individual& b) { 
                return a.fitness < b.fitness; 
        });

        // 4. Reduzir para o tamanho original
        if (new_population.size() > current_population.size()) {
            new_population.resize(current_population.size());
        }

        // 5. Atualizar Incumbent (Melhor global)
        if (new_population[0].fitness < incumbent.fitness) {
            incumbent = new_population[0];
        }

        return {new_population, incumbent};
    }

    void serial_SGS(individual &individual) {

        for (auto& node : nodes) {

            auto it = find(individual.activity_list.begin(),
                           individual.activity_list.end(),
                           node.id);

            if(it != individual.activity_list.end()) {
                node.priority_value = distance(individual.activity_list.begin(), it);
            } else {
                node.priority_value = number_of_jobs + 1.0;
            }

            node.start_time = -1;
            node.finish_time = -1;
            node.started = false;
            node.finished = false;
            node.scheduled = false;  
        }

            vector<vector<int>> R_kt;

            for (int k : renewable_resource_availability) {
                R_kt.push_back(vector<int>(horizon, k));
            }

            unordered_set<int> scheduled_activities;
            node& starting_node = nodes[0];
            starting_node.start_time = 0;
            starting_node.finish_time = 0;
            starting_node.started = true;
            starting_node.finished = true;
            starting_node.scheduled = true;

            scheduled_activities.insert(starting_node.id);

            vector<int> eligibles = starting_node.successors;  
            sort(eligibles.begin(), eligibles.end(), [&](int a, int b) {
                return nodes[a].priority_value < nodes[b].priority_value;
            });

            vector<int> resource_profile_changes;
            resource_profile_changes.push_back(0);

            while (scheduled_activities.size() != number_of_jobs) {

                int selected_id = eligibles[0];
                node& selected_node = nodes[selected_id];

                int current_t = -1;
                for (int pred_id : selected_node.predecessors) {
                    node& pred = nodes[pred_id];
                    current_t = max(current_t, pred.start_time + pred.duration_time);
                }

                while (!selected_node.scheduled) {
                    bool violation = false;

                    for (int k = 0; k < number_of_renewable_resources; k++) {
                        for (int t = current_t; t < current_t + selected_node.duration_time; t++) {

                            if (selected_node.renewable_resource_requirements[k] > R_kt[k][t]) {
                                violation = true;

                                int new_t = INT_MAX;
                                for (int c : resource_profile_changes) {
                                    if (c > current_t) new_t = min(new_t, c);
                                }
                                current_t = new_t;
                                break;
                            }
                        }
                        if (violation) break;
                    }

                    if (!violation) {

                        selected_node.scheduled  = true;
                        selected_node.start_time = current_t;
                        selected_node.finish_time = current_t + selected_node.duration_time;
                        selected_node.started = true;
                        selected_node.finished = true;

                        for (int k = 0; k < number_of_renewable_resources; k++) {
                            for (int t = current_t; t < current_t + selected_node.duration_time; t++) {
                                R_kt[k][t] -= selected_node.renewable_resource_requirements[k];
                            }
                        }

                        resource_profile_changes.clear();
                        for (node& nd : nodes) {
                            if (nd.finish_time >= 0)
                                resource_profile_changes.push_back(nd.finish_time);
                        }

                        sort(resource_profile_changes.begin(), resource_profile_changes.end());
                        resource_profile_changes.erase(
                            unique(resource_profile_changes.begin(), resource_profile_changes.end()),
                            resource_profile_changes.end()
                        );
                    }
                }

                scheduled_activities.insert(selected_node.id);

                eligibles.erase(
                    remove(eligibles.begin(), eligibles.end(), selected_node.id),
                    eligibles.end()
                );

                for (int succ_id : selected_node.successors) {
                    node& succ = nodes[succ_id];

                    bool all_preds_scheduled = true;
                    for (int pred_id : succ.predecessors) {
                        if (!nodes[pred_id].scheduled) {
                            all_preds_scheduled = false;
                            break;
                        }
                    }

                    if (all_preds_scheduled &&
                        find(eligibles.begin(), eligibles.end(), succ_id) == eligibles.end()) {
                        eligibles.push_back(succ_id);
                    }
                }

                sort(eligibles.begin(), eligibles.end(), [&](int a, int b){
                    return nodes[a].priority_value < nodes[b].priority_value;
                });
            }

            int best = -1;
            for (int id : scheduled_activities) {
                best = max(best, nodes[id].finish_time);
            }
            individual.fitness = best;
    }

    /**
     * @brief Parallel Schedule Generation Scheme (SGS).
     * Constrói um cronograma iterando sobre o tempo. Em cada ponto de decisão (t),
     * tenta agendar o máximo de atividades elegíveis possível respeitando os recursos.
     * Quando nada mais cabe em (t), avança para o próximo tempo de término de uma atividade.
     * * @param individual O indivíduo (cromossomo) a ser avaliado. Passado por referência para atualizar o fitness.
     */
    void parallel_SGS(individual &individual) {
        
        vector<int> priority_map(number_of_jobs);
        for(size_t i = 0; i < individual.activity_list.size(); ++i) {
            priority_map[individual.activity_list[i]] = (int)i;
        }

        for (auto& node : nodes) {
            node.priority_value = priority_map[node.id];

            node.start_time = -1;
            node.finish_time = -1;
            node.started = false;
            node.finished = false;
            node.scheduled = false;
        }

        vector<vector<int>> matriz_recurso_kt;
        for(int k : renewable_resource_availability) {
            // Nota: Se o horizonte for muito curto para uma solução ruim,
            // isso pode causar problemas se não tratarmos o limite abaixo.
            matriz_recurso_kt.push_back(vector<int>(horizon + 1, k)); 
        }

        // Marcar o nó fonte (job 1, index 0) como já agendado no tempo 0.
        // Sem isso, o laço de elegíveis nunca irá considerar sucessores do
        // nó fonte porque sua finish_time inicialmente é -1, levando o
        // algoritmo a avançar o tempo até o horizonte sem agendar nada.
        if (!nodes.empty()) {
            node &starting_node = nodes[0];
            starting_node.start_time = 0;
            starting_node.finish_time = 0;
            starting_node.scheduled = true;
        }

        int scheduled_count = 1; // já contamos o nó fonte
        int current_time = 0;

        vector<int> active_jobs;

        while (scheduled_count < number_of_jobs) {
            
            if (current_time >= horizon) {
                individual.fitness = (double)horizon * 2.0;
                return;
            }

            vector<int> eligibles;

            for (const auto& node : nodes) {
                if (!node.scheduled) {
                    bool predecessors_finished = true;
                    for (int pred_id : node.predecessors) {
                        if (nodes[pred_id].finish_time == -1 || nodes[pred_id].finish_time > current_time) {
                            predecessors_finished = false;
                            break;
                        }
                    }
                    if (predecessors_finished) {
                        eligibles.push_back(node.id);
                    }
                }
            }

            sort(eligibles.begin(), eligibles.end(), [&](int a, int b) {
                return nodes[a].priority_value < nodes[b].priority_value;
            });

            for (int node_id : eligibles) {
                node& curr_node = nodes[node_id];
                bool can_schedule = true;

                if (current_time + curr_node.duration_time > horizon) {
                    can_schedule = false; 
                } else {
                    for (int k = 0; k < number_of_renewable_resources; k++) {
                        for (int t = current_time; t < current_time + curr_node.duration_time; t++) {
                            if (curr_node.renewable_resource_requirements[k] > matriz_recurso_kt[k][t]) {
                                can_schedule = false;
                                break;
                            }
                        }
                        if (!can_schedule) break;
                    }
                }

                if (can_schedule) {
                    curr_node.scheduled = true;
                    curr_node.start_time = current_time;
                    curr_node.finish_time = current_time + curr_node.duration_time;
                    
                    for (int k = 0; k < number_of_renewable_resources; k++) {
                        for (int t = current_time; t < current_time + curr_node.duration_time; t++) {
                            matriz_recurso_kt[k][t] -= curr_node.renewable_resource_requirements[k];
                        }
                    }

                    active_jobs.push_back(node_id);
                    scheduled_count++;
                }
            }

            // Se agendamos tudo, sair
            if (scheduled_count == number_of_jobs) break;

            // Avançar o Tempo (Time Advance)
            int next_time = horizon + 1; // Valor sentinela maior que horizonte
            bool found_next = false;
            
            // Limpa jobs ativos que já terminaram e busca o próximo salto de tempo
            for (auto it = active_jobs.begin(); it != active_jobs.end(); ) {
                int job_finish = nodes[*it].finish_time;
                
                if (job_finish > current_time) {
                    if (job_finish < next_time) {
                        next_time = job_finish;
                        found_next = true;
                    }
                    ++it;
                } else {
                    it = active_jobs.erase(it); 
                }
            }

            if (found_next) {
                current_time = next_time;
            } else {
                // Se nenhum job ativo ditar o futuro, avançamos 1 unidade
                // Isso acontece quando recursos estão bloqueados mas nenhum job está em andamento
                current_time++;
            }
        }

        // Calcular Fitness (Makespan)
        int max_finish = 0;
        for (const auto& node : nodes) {
            if (node.finish_time > max_finish) {
                max_finish = node.finish_time;
            }
        }
        individual.fitness = max_finish;
    }

    individual solve_instance_via_ga(int pop_size, int generations, double mut_prob, void (project::*sgs)(individual &)) {

        this->population.clear();
        this->population = create_initial_population(pop_size);

        individual best_global;

        // Avaliação inicial
        for (size_t i = 0; i < population.size(); ++i) {
            auto &ind = population[i];
            (this->*sgs)(ind);
            // preencher mapas de tempos no indivíduo para posterior visualização
            ind.start_times.clear(); ind.finish_times.clear();
            for (const auto &nd : nodes) {
                ind.start_times[nd.id] = nd.start_time;
                ind.finish_times[nd.id] = nd.finish_time;
            }
            if (ind.fitness < best_global.fitness) best_global = ind;
            if ((i + 1) % 10 == 0) cout << "[GA] Avaliado " << (i + 1) << "/" << population.size() << " individuos" << endl;
        }

        for (int g = 0; g < generations; ++g) {
            vector<individual> off = crossover(population);

            cout << "[GA]   Mutacao..." << endl;
            off = mutate(off, mut_prob);

            for (auto &ind : off) {
                (this->*sgs)(ind);
                ind.start_times.clear(); ind.finish_times.clear();
                for (const auto &nd : nodes) {
                    ind.start_times[nd.id] = nd.start_time;
                    ind.finish_times[nd.id] = nd.finish_time;
                }
            }

            // Elitismo + Seleção (Rank and Reduce Simplificado)
            population.insert(population.end(), off.begin(), off.end());
            sort(population.begin(), population.end(), [](const individual &a, const individual &b) {
                return a.fitness < b.fitness;
            });
            population.resize(pop_size);

            if (population[0].fitness < best_global.fitness) {
                best_global = population[0];
            }
        }
        cout << "[GA] Algoritmo finalizado! Melhor fitness: " << best_global.fitness << endl;
        return best_global;
    }

    
    // --- Funções utilitárias para visualização do cronograma ---
    int extract_num_patients_from_filename(const string &filename) {
        // tenta encontrar um número antes da palavra 'pacientes' ou 'paciente' no nome do arquivo
        size_t pos = filename.find("pacientes");
        if (pos == string::npos) pos = filename.find("paciente");
        if (pos == string::npos) return 0;

        // procurar dígitos à esquerda
        int i = (int)pos - 1;
        while (i >= 0 && !isdigit((unsigned char)filename[i])) i--;
        if (i < 0) return 0;
        int j = i;
        while (j >= 0 && isdigit((unsigned char)filename[j])) j--;

        string num = filename.substr(j + 1, i - j);
        try {
            return stoi(num);
        } catch (...) {
            return 0;
        }
    }

    void print_schedule_console(individual &ind, const string &instance_name) {
        cout << "\n--- Cronograma para instância: " << instance_name << " ---" << endl;

        int num_patients = extract_num_patients_from_filename(instance_name);
        int total_real_activities = number_of_jobs - 2; // remover source/sink

        if (num_patients > 0 && total_real_activities % num_patients == 0) {
            int activities_per_patient = total_real_activities / num_patients;
            vector<vector<tuple<int,int,int,int>>> by_patient(num_patients + 1);

            for (const auto &p : ind.start_times) {
                int id = p.first; // 0-based
                if (id == 0 || id == number_of_jobs - 1) continue; // ignorar source/sink
                int start = p.second;
                int finish = ind.finish_times.count(id) ? ind.finish_times.at(id) : (start + nodes[id].duration_time);
                int printed_id = id + 1; // para compatibilidade com parser (1-based)
                int paciente_num = ((printed_id - 2) / activities_per_patient) + 1;
                if (paciente_num < 1 || paciente_num > num_patients) paciente_num = 0;
                if (paciente_num == 0) continue;
                by_patient[paciente_num].push_back(make_tuple(printed_id, start, finish, nodes[id].duration_time));
            }

            for (int p = 1; p <= num_patients; ++p) {
                cout << "\nPaciente " << p << ":" << endl;
                auto &list = by_patient[p];
                sort(list.begin(), list.end(), [](const tuple<int,int,int,int> &a, const tuple<int,int,int,int> &b){
                    return get<1>(a) < get<1>(b);
                });
                for (auto &t : list) {
                    cout << "  Atividade " << get<0>(t) << " | Inicia: " << get<1>(t) << " | Termina: " << get<2>(t) << " | Dur: " << get<3>(t) << endl;
                }
            }
        } else {
            // fallback: imprime lista plana de tarefas com tempos
            cout << "(Formato padrão) Jobs | Start | Finish | Dur" << endl;
            vector<int> ids;
            for (const auto &p : ind.start_times) ids.push_back(p.first);
            sort(ids.begin(), ids.end());
            for (int id : ids) {
                int start = ind.start_times.at(id);
                int finish = ind.finish_times.count(id) ? ind.finish_times.at(id) : (start + nodes[id].duration_time);
                cout << "Job " << id + 1 << " | " << start << " | " << finish << " | " << nodes[id].duration_time << endl;
            }
        }

        cout << "--- Fim do cronograma ---\n" << endl;
    }

    private:
    // --- Métodos Auxiliares Internos do project ---

    /**
     * @brief Forward Pass: Calcula Earliest Start (ES) e Earliest Finish (EF).
     * Itera de 0 a N-1.
     */
    void calculate_forward_pass() {
        nodes[0].earliest_start = 0;
        nodes[0].earliest_finish = 0;

        for (int i = 1; i < number_of_jobs; i++) {
            int max_pred_ef = 0;

            for (int predecessor_id : nodes[i].predecessors) {
                if (nodes[predecessor_id].earliest_finish > max_pred_ef) {
                    max_pred_ef = nodes[predecessor_id].earliest_finish;
                }
            }

            nodes[i].earliest_start = max_pred_ef;
            nodes[i].earliest_finish = max_pred_ef + nodes[i].duration_time;
        }
    }

    /**
     * @brief Backward Pass: Calcula Latest Start (LS) e Latest Finish (LF).
     * Itera de N-1 a 0.
     */
    void calculate_backward_pass() {
        node &finish_node = nodes[number_of_jobs - 1];
        finish_node.latest_start = horizon;
        finish_node.latest_finish = horizon;

        for (int i = number_of_jobs - 2; i >= 0; i--) {
            int min_succ_ls = horizon;

            for (int successor_id : nodes[i].successors) {
                if (nodes[successor_id].latest_start < min_succ_ls) {
                    min_succ_ls = nodes[successor_id].latest_start;
                }
            }

            nodes[i].latest_start = min_succ_ls - nodes[i].duration_time;
            nodes[i].latest_finish = min_succ_ls;
        }
    }


    /**
     * @brief Concatena o caminho do diretório com o nome do arquivo de forma
     * segura. O padrão do arquivo é para linux.
     * * Verifica se o caminho já possui uma barra separadora no final. Se não
     * tiver, adiciona a barra apropriada ('/') antes de juntar com o nome do
     * arquivo.
     * * @param instance_filepath Caminho base do diretório (ex: "Instances" ou
     * "C:\\Dados").
     * @param instance_name Nome do arquivo (ex: "j120.sm").
     * @return string O caminho completo pronto para ser aberto (ex:
     * "Instances/j120.sm").
     */
    string concatanate_path_file_name(string instance_filepath,
                                      string instance_name) {

        if (instance_filepath.empty())
            return instance_name;

        // Se não terminar com '/', adiciona.
        // Nota: Se terminar com '\' (Windows), vai virar "\/", mas o Windows
        // aceita.
        if (instance_filepath.back() != '/') {
            instance_filepath += "/";
        }
        instance_filepath += instance_name;

        return instance_filepath;
    }

    /**
     * @brief Carrega todo o conteúdo de um arquivo para a memória.
     * * Lê o arquivo linha por linha e armazena em um vetor de strings,
     * permitindo acesso aleatório às linhas (ex: acessar linha X diretamente).
     * * @param full_path O caminho completo para o arquivo a ser lido.
     * @return vector<string> Um vetor contendo as linhas do arquivo. Retorna um
     * vetor vazio {} em caso de erro.
     */
    vector<string> load_file_in_memory(string full_path) {

        ifstream file(full_path);

        if (!file.is_open()) {
            cerr << "Erro: Não foi possível abrir o arquivo " << full_path << endl;
            return {};
        }

        vector<string> file_lines;
        string line;
        while (getline(file, line)) {
            file_lines.push_back(line);
        }
        file.close();

        return file_lines;
    }
};


int main() {
    string folder_path = "../instances/instancias_geradas";
    string output_csv = "resultado_experimento.csv";
    
    // Parâmetros do GA de acordo com o artigo
    int pop_size = 40;
    int gens = 25;
    double mut = 0.05;

    ofstream csv(output_csv);
    if (!csv.is_open()) {
        cerr << "Erro ao criar arquivo CSV." << endl;
        return 1;
    }

    // Cabeçalho do CSV
    csv << "Instance,NumJobs,LowerBound(CPM),BestMakespan,Gap(%),Time(ms)\n";
    cout << "Iniciando experimentos...\n" << endl;

    project p;

    // Iterar sobre arquivos na pasta
    try {
        for (const auto& entry : fs::directory_iterator(folder_path)) {
            if (entry.path().extension() == ".sm") {
                string file_path = entry.path().string();
                string file_name = entry.path().filename().string();

                cout << "Processando: " << file_name << "... ";

                // Carregar projeto
                p.read_project(file_path);
                
                if (p.number_of_jobs == 0) {
                    cout << "[ERRO Lendo]" << endl;
                    continue;
                }

                // Medir tempo
                auto start = chrono::high_resolution_clock::now();

                // Rodar GA (retorna o indivíduo ótimo com cronograma preenchido)
                individual best = p.solve_instance_via_ga(pop_size, gens, mut, &project::parallel_SGS);

                auto end = chrono::high_resolution_clock::now();
                auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

                double result = best.fitness;

                // Calcular Métricas
                double lb = (double)p.cpm_lower_bound;
                double gap = 0.0;
                if (lb > 0) gap = ((result - lb) / lb) * 100.0;

                // Escrever no CSV
                csv << file_name << ","
                    << p.number_of_jobs << ","
                    << lb << ","
                    << result << ","
                    << fixed << setprecision(2) << gap << ","
                    << duration << "\n";

                cout << "Makespan: " << result << " | Gap: " << gap << "% | Tempo: " << duration << "ms" << endl;
                // Imprimir cronograma no console (agrupado por paciente quando o nome do arquivo indicar)
                p.print_schedule_console(best, file_name);
            }
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "Erro ao acessar diretorio: " << e.what() << endl;
        return 1;
    }

    csv.close();
    cout << "\nExperimento finalizado! Resultados salvos em: " << output_csv << endl;

    return 0;
}
