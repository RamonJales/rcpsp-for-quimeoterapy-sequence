#include <iostream>
#include <vector>
#include <limits>
#include <map>
#include <random>

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
     * Inicializa os IDs e tempos com -1 (indicando "não definido") e flags como false.
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
 * @brief Representa um Indivíduo (Cromossomo) na população do Algoritmo Genético.
 * * Um indivíduo carrega uma permutação de tarefas (genótipo) e o agendamento resultante (fenótipo).
 */
struct individual
{
    /// O valor da função objetivo (Makespan).
    double fitness;

    vector<int> activity_list;

    /// Mapa {ID da Tarefa -> Tempo de Início}. Representa quando cada tarefa começa nesta solução.
    map<int, int> start_times;
    map<int, int> finish_times;

    /**
     * @brief Construtor padrão.
     * Inicializa o fitness com infinito positivo (pior caso para minimização).
     */
    individual() {
        fitness = numeric_limits<double>::infinity();
    }

    /**
     * @brief Verifica se a activity_list respeita as restrições de precedência topológica.
     * * Garante que nenhum sucessor apareça antes de seu predecessor na lista de prioridades.
     * Isso é crucial para que o SGS Serial funcione corretamente.
     * * @param all_nodes Referência ao vetor principal contendo os dados de todos os nós do projeto.
     * @return true Se a lista for viável (topologicamente ordenada).
     * @return false Se houver violação de precedência.
     */
    bool check_precedence_feasibility(vector<node>& all_nodes) {
        vector<int> position_in_list(all_nodes.size(), -1);

        for (size_t i = 0; i < activity_list.size(); ++i) {
            position_in_list[activity_list[i]] = i;
        }

        for (int node_id : activity_list) {
            int current_position = position_in_list[node_id];

            const node& current_node = all_nodes[node_id];

            for (int predecessor_id : current_node.predecessors) {
                if (position_in_list[predecessor_id] != -1) {
                    if(position_in_list[predecessor_id] > current_position) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    void restore_precedence_of_activity_list() {}
};

struct project
{
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
        if(nodes.empty()) return;

        calculate_forward_pass();
        calculate_backward_pass();
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
        node& finish_node = nodes[number_of_jobs - 1];
        finish_node.latest_start = horizon;
        finish_node.latest_finish = horizon;

        for (int i = number_of_jobs - 2; i >= 0; i--) {
            int min_succ_ls = horizon;

            for (int successor_id : nodes[i].successors) {
                if(nodes[successor_id].latest_start < min_succ_ls) {
                    min_succ_ls = nodes[successor_id].latest_start;
                }
            }

            nodes[i].latest_start = min_succ_ls - nodes[i].duration_time;
            nodes[i].latest_finish = min_succ_ls;
        }
    }
};
