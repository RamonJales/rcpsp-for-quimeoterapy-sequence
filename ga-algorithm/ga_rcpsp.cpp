#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

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
