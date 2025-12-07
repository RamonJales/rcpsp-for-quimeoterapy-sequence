from algorithm import gerar_cronograma
from view_calendar import imprimir_cronograma
from parse_sm import parse_sm_file
import os

n_pacientes = input("Digite o número de pacientes (5, 25, 40, 50): ")

if n_pacientes not in ['5', '25', '40', '50']:
    print("Número inválido. Por favor, escolha entre 5, 25, 40 ou 50.")
    exit(1)

n = int(n_pacientes)

# Construir caminho de forma robusta: a pasta 'instances' está no diretório pai do pacote
base_dir = os.path.dirname(os.path.abspath(__file__))
caminho_do_arquivo = os.path.normpath(os.path.join(base_dir, '..', 'instances', 'instancias_geradas', f'folfiri_{n}_pacientes.sm'))

if not os.path.exists(caminho_do_arquivo):
    print(f"Arquivo de instância não encontrado: {caminho_do_arquivo}")
    exit(1)

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