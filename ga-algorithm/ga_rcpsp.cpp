#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <random>
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

    vector<int> renewable_resource_availability;
    vector<individual> population;

    vector<node> nodes;

    project() {
        random_device rd;
        rng = mt19937(rd());
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
    void read_project(string instance_filepath, string instance_name) {
        string full_path = concatanate_path_file_name(instance_filepath, instance_name);
        vector<string> file_lines = load_file_in_memory(full_path);

        if (file_lines.empty()) return;

        for (size_t line_index = 0; line_index < file_lines.size(); ++line_index) {
            string current_line = file_lines[line_index];

            // 1. Parsing do Cabeçalho (Jobs, Horizon, Resources)
            // O arquivo fornecido usa "jobs (incl. supersource/sink )"
            if (current_line.find("jobs (incl. supersource/sink )") != string::npos) {
                // Ler número de jobs da linha ATUAL
                stringstream ss_jobs(current_line);
                string temp;
                int val;
                while (ss_jobs >> temp) {
                    if (stringstream(temp) >> val) number_of_jobs = val;
                }
                number_of_nondummy_jobs = number_of_jobs - 2;

                // Ler Horizon da PRÓXIMA linha (+1)
                if (line_index + 1 < file_lines.size()) {
                    stringstream ss_hor(file_lines[line_index + 1]);
                    while (ss_hor >> temp) {
                        if (stringstream(temp) >> val) horizon = val;
                    }
                }

                // Ler Recursos Renováveis da linha seguinte (+2)
                // Ex: "renewable resources: 4"
                if (line_index + 2 < file_lines.size()) {
                    stringstream ss_res(file_lines[line_index + 2]);
                    vector<int> numbers;
                    while (ss_res >> temp) {
                        if (stringstream(temp) >> val) numbers.push_back(val);
                    }
                    if (!numbers.empty()) {
                        number_of_renewable_resources = numbers.back();
                    }
                }

                // Redimensionar vetor de nós
                nodes.clear();
                nodes.resize(number_of_jobs);
                for (int i = 0; i < number_of_jobs; ++i) {
                    nodes[i].id = i;
                }
            }
            // 2. Parsing das Precedências
            else if (current_line.find("PRECEDENCE RELATIONS:") != string::npos) {
                // Cabeçalho está em +1 ("jobnr. mode successors..."), dados começam em +2
                size_t starting_line_index = line_index + 2;

                for (int i = 0; i < number_of_jobs; ++i) {
                    if (starting_line_index + i >= file_lines.size()) break;

                    stringstream ss(file_lines[starting_line_index + i]);
                    int job_idx, mode, num_successors;
                    
                    // Formato: jobnr mode num_succ succ1 succ2 ...
                    ss >> job_idx >> mode >> num_successors;

                    for (int j = 0; j < num_successors; ++j) {
                        int succ_id;
                        ss >> succ_id;
                        // Conversão de 1-based para 0-based
                        int successor_node_name = succ_id - 1;
                        nodes[i].successors.push_back(successor_node_name);
                    }
                }

                // Preencher predecessores automaticamente (baseado nos sucessores lidos)
                for (int i = 0; i < number_of_jobs; ++i) {
                    for (int succ_id : nodes[i].successors) {
                        nodes[succ_id].predecessors.push_back(i);
                    }
                }
            }
            // 3. Parsing das Durações e Demandas de Recursos
            else if (current_line.find("REQUESTS/DURATIONS:") != string::npos) {
                // Cabeçalho está em +1 ("jobnr. mode duration..."), dados começam em +2
                // (Nota: O arquivo original pulava +3, o que causaria erro aqui)
                size_t starting_line_index = line_index + 2;

                for (int i = 0; i < number_of_jobs; ++i) {
                    if (starting_line_index + i >= file_lines.size()) break;

                    stringstream ss(file_lines[starting_line_index + i]);
                    int job_idx, mode, duration;
                    
                    // Formato: jobnr mode duration R1 R2 R3 R4
                    ss >> job_idx >> mode >> duration;
                    
                    // Como job_idx é lido sequencialmente, assumimos que nodes[i] corresponde à linha
                    nodes[i].duration_time = duration;

                    int req;
                    // Lê apenas a quantidade de recursos que definimos anteriormente
                    for (int k = 0; k < number_of_renewable_resources; ++k) {
                        if (ss >> req) {
                            nodes[i].renewable_resource_requirements.push_back(req);
                        }
                    }
                }
            }
            // 4. Parsing da Disponibilidade de Recursos
            else if (current_line.find("RESOURCEAVAILABILITIES:") != string::npos) {
                size_t starting_line_index = line_index + 2;
                if (starting_line_index < file_lines.size()) {
                     stringstream ss(file_lines[starting_line_index]);
                     int avail;
                     renewable_resource_availability.clear();
                     while (ss >> avail) {
                         renewable_resource_availability.push_back(avail);
                     }
                     // Ajuste de segurança caso leia mais do que o esperado
                     if (renewable_resource_availability.size() > (size_t)number_of_renewable_resources) {
                         renewable_resource_availability.resize(number_of_renewable_resources);
                     }
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

        // Cabeçalho da tabela simples
        // Ajuste a largura (setw) conforme necessário se usar <iomanip>
        for (const auto& node : nodes) {
            // ID e Duração
            cout << "Job " << node.id + 1; // Mostrando 1-based para bater com o arquivo
            cout << "\t| Dur: " << node.duration_time;
            
            // Requisitos de Recursos
            cout << "\t| Res: [";
            for (size_t k = 0; k < node.renewable_resource_requirements.size(); ++k) {
                cout << node.renewable_resource_requirements[k] << (k < node.renewable_resource_requirements.size() - 1 ? " " : "");
            }
            cout << "]";

            // Sucessores
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
            (this->*sgs)(indiv); // Sintaxe para chamar método membro
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

        for (auto& node : nodes) {
            auto it = find(individual.activity_list.begin(), individual.activity_list.end(), node.id);

            if (it != individual.activity_list.end()) {
                node.priority_value = distance(individual.activity_list.begin(), it);
            } else {
                node.priority_value = number_of_jobs + 1;
            }

            node.start_time = -1;
            node.finish_time = -1;
            node.started = false;
            node.finished = false;
            node.scheduled = false;
        }

        // matriz_recurso_kt[k][t] guarda a capacidade disponível do recurso k no tempo t
        vector<vector<int>> matriz_recurso_kt;
        for(int k : renewable_resource_availability) {
            matriz_recurso_kt.push_back(vector<int>(horizon, k));
        }

        int scheduled_count = 0;
        int current_time = 0;

        vector<int> active_jobs;

        while (scheduled_count < number_of_jobs) {
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

            // 4.3 Tentar agendar as elegíveis no tempo atual (current_time)
            for (int node_id : eligibles) {
                node& curr_node = nodes[node_id];
                bool can_schedule = true;

                // Verificar restrições de recursos de t até t + duração
                if (current_time + curr_node.duration_time > horizon) {
                    can_schedule = false; // Estourou o horizonte
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

                // Se houver recursos, agenda a tarefa
                if (can_schedule) {
                    curr_node.scheduled = true;
                    curr_node.start_time = current_time;
                    curr_node.finish_time = current_time + curr_node.duration_time;
                    
                    // Atualizar matriz de recursos
                    for (int k = 0; k < number_of_renewable_resources; k++) {
                        for (int t = current_time; t < current_time + curr_node.duration_time; t++) {
                            matriz_recurso_kt[k][t] -= curr_node.renewable_resource_requirements[k];
                        }
                    }

                    active_jobs.push_back(node_id);
                    scheduled_count++;
                }
            }


        // 4.4 Avançar o Tempo (Time Advance)
            // Se já agendamos tudo, podemos parar
            if (scheduled_count == number_of_jobs) break;

            // O próximo ponto de decisão é o menor tempo de término entre as atividades ativas
            // que terminam APÓS o tempo atual.
            int next_time = horizon;
            
            // Filtra jobs ativos que já terminaram no passado para manter a lista limpa (opcional, mas bom para performance)
            // E busca o próximo tempo de salto
            bool found_next = false;
            for (auto it = active_jobs.begin(); it != active_jobs.end(); ) {
                int job_finish = nodes[*it].finish_time;
                
                if (job_finish > current_time) {
                    if (job_finish < next_time) {
                        next_time = job_finish;
                        found_next = true;
                    }
                    ++it;
                } else {
                    // Job já terminou antes ou agora, não dita o futuro salto, mas libera recursos logicos
                    // (no SGS paralelo a matriz R_kt já cuidou da liberação física)
                    it = active_jobs.erase(it); 
                }
            }

            if (found_next) {
                current_time = next_time;
            } else {
                // Se não encontrou próximo tempo e ainda faltam jobs, algo deu errado (ou só restam jobs sem duração?)
                // Avança 1 unidade para evitar loop infinito em casos degenerados
                if (active_jobs.empty() && !eligibles.empty()) {
                   // Caso onde recursos estão bloqueados mas nenhum job está "ativo" (ex: setup time ou delay externo)
                   // No RCPSP padrão isso raramente ocorre se a lógica estiver certa.
                   // Vamos buscar o menor finish time geral > current_time
                   int min_finish = horizon;
                   for(const auto& n : nodes) {
                       if (n.scheduled && n.finish_time > current_time) {
                           min_finish = min(min_finish, n.finish_time);
                       }
                   }
                   current_time = min_finish;
                } else {
                     current_time++;
                }
            }
        }

        // 5. Calcular Fitness (Makespan)
        int max_finish = 0;
        for (const auto& node : nodes) {
            if (node.finish_time > max_finish) {
                max_finish = node.finish_time;
            }
        }
        individual.fitness = max_finish;
    }

    void solve_instance_via_ga(int pop_size, int number_of_generations, double mutation_probability, void (project::*sgs)(individual &)) {
        individual incumbent;

        this->population = create_initial_population(pop_size);

        for(auto &indiv : this->population) {
            (this->*sgs)(indiv);
            
            if (indiv.fitness < incumbent.fitness) {
                incumbent = indiv;
            }
        }

        for (int gen = 0; gen < number_of_generations; ++gen) {
            cout << "gen: " << gen << endl;
            cout << "incumbent fitness: " << incumbent.fitness << endl;
            cout << "--" << endl;

            // Crossover
            vector<individual> offsprings = crossover(this->population);

            // Mutação
            offsprings = mutate(offsprings, mutation_probability);

            // Rank and Reduce (Seleção e Sobrevivência)
            // Retorna par: {nova_populacao, novo_incumbent}
            pair<vector<individual>, individual> result = rank_and_reduce(this->population, offsprings, incumbent, sgs);
            
            this->population = result.first;
            incumbent = result.second;
        }

        cout << "finished GA with fitness of: " << incumbent.fitness << endl;
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
    project p;

    string path = "../instances/instancias_geradas"; 
    string file = "folfiri_25_pacientes.sm";

    cout << ">>> Tentando ler o arquivo: " << path << "/" << file << endl;

    // 3. Chama o método de leitura
    p.read_project(path, file);

    // 4. (Opcional) Calcula o caminho crítico para verificar se a topologia (sucessores/predecessores) ficou consistente
    // Isso preenche ES, EF, LS, LF
    p.forward_backward_scheduling();

    // 5. Imprime os dados para conferência
    p.print_project();

    // Verificação rápida se leu algo
    if (p.number_of_jobs > 0) {
        cout << "\n>>> Sucesso! O projeto foi lido e carregado na memoria." << endl;
        cout << "Horizonte do projeto: " << p.horizon << endl;
    } else {
        cerr << "\n>>> Erro: Parece que o projeto nao foi carregado. Verifique o caminho do arquivo." << endl;
    }

    return 0;
}
