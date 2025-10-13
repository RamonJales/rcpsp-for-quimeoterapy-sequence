from algorithm import gerar_cronograma
from view_calendar import imprimir_cronograma
from parse_sm import parse_sm_file

for i in [5, 25, 40, 50]:

    caminho_do_arquivo = f"instances/instancias_geradas/folfiri_{i}_pacientes.sm"

    atividades, precedencias, recursos = parse_sm_file(caminho_do_arquivo)

    incompatibilidades = [
        (0, 1),
        (2, 3)  
    ]

    cronograma = gerar_cronograma(atividades, recursos, precedencias, incompatibilidades)
    
    print("\n" + "="*80)
    print(f"      RELATÓRIO DETALHADO DO CRONOGRAMA PARA {i} PACIENTES")
    print("="*80)

    imprimir_cronograma(cronograma, atividades, precedencias, num_pacientes=i)
    print("\n\n")



