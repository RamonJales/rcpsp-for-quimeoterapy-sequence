from branch_and_bound import solve_rcpsp_bb
from parse_sm import parse_sm_file
from view_calendar import imprimir_cronograma
import sys


def main():
    print("\n" + "="*80)
    print("  BRANCH AND BOUND - RCPSP COM INCOMPATIBILIDADES")
    print("  Sequenciamento de Protocolos de Quimioterapia")
    print("="*80)
    
    # Solicita número de pacientes
    n_pacientes = input("\nDigite o número de pacientes (5, 25, 40, 50): ")
    
    if n_pacientes not in ['5', '25', '40', '50']:
        print("Número inválido. Por favor, escolha entre 5, 25, 40 ou 50.")
        sys.exit(1)
    
    n = int(n_pacientes)
    caminho_do_arquivo = f"instances/instancias_geradas/folfiri_{n}_pacientes.sm"
    
    print(f"\nCarregando instância: {caminho_do_arquivo}")
    
    # Parse do arquivo .sm
    atividades, precedencias, recursos = parse_sm_file(caminho_do_arquivo)
    
    print(f"Atividades: {len(atividades)}")
    print(f"Precedências: {len(precedencias)}")
    print(f"Recursos: {recursos}")
    
    incompatibilidades = [
        (2, 3),   # Exemplo: atividades 2 e 3 são incompatíveis
        (9, 10),  # Exemplo: atividades 9 e 10 são incompatíveis
    ]
    
    print(f"Incompatibilidades: {len(incompatibilidades)}")
    
    # Solicita limite de tempo
    time_limit_input = input("\nLimite de tempo em segundos (padrão: 600): ")
    if time_limit_input.strip():
        time_limit = int(time_limit_input)
    else:
        time_limit = 600
    
    print(f"\nIniciando Branch and Bound (limite: {time_limit}s)...")
    print("-"*80)
    
    # Executa Branch and Bound
    makespan, cronograma, stats = solve_rcpsp_bb(
        atividades,
        recursos,
        precedencias,
        incompatibilidades,
        time_limit=time_limit,
        verbose=True
    )
    
    # Exibe resultados
    print("\n" + "="*80)
    print("  RESULTADOS DO BRANCH AND BOUND")
    print("="*80)
    print(f"Makespan ótimo: {makespan:.1f}")
    print(f"Nós explorados: {stats['nodes_explored']}")
    print(f"Nós podados: {stats['nodes_pruned']}")
    print(f"Tempo de execução: {stats['time']:.2f}s")
    print(f"Solução ótima: {'Sim' if stats['optimal'] else 'Não (limite de tempo)'}")
    print("="*80)
    
    # Exibe cronograma se disponível
    if cronograma:
        print("\n" + "="*80)
        print(f"  CRONOGRAMA DETALHADO - {n} PACIENTES")
        print("="*80)
        imprimir_cronograma(cronograma, atividades, precedencias, num_pacientes=n)
    else:
        print("\nNenhuma solução factível encontrada.")
    
    
    print(f"Makespan B&B:   {makespan:.1f}")

    print("="*80)


if __name__ == "__main__":
    main()
