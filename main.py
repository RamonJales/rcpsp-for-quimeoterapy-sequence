from algorithm import gerar_cronograma
from view_calendar import imprimir_cronograma
from parse_sm import parse_sm_file

n_pacientes = input("Digite o número de pacientes (5, 25, 40, 50): ")

if n_pacientes not in ['5', '25', '40', '50']:
    print("Número inválido. Por favor, escolha entre 5, 25, 40 ou 50.")
    exit(1)

n = int(n_pacientes)
caminho_do_arquivo = f"instances/instancias_geradas/folfiri_{n}_pacientes.sm"

atividades, precedencias, recursos = parse_sm_file(caminho_do_arquivo)

incompatibilidades = [
    (0, 1),
    (2, 3),
    (4, 5)
]

cronograma = gerar_cronograma(atividades, recursos, precedencias, incompatibilidades)
    
print("\n" + "="*80)
print(f"      RELATÓRIO DETALHADO DO CRONOGRAMA PARA {n} PACIENTES")
print("="*80)

imprimir_cronograma(cronograma, atividades, precedencias, num_pacientes=n)
print("\n\n")



