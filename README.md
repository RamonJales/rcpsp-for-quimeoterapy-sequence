# RCPSP para o sequenciamento de protocolos de quimioterapia

O Problema de Agendamento de Projetos com Recursos Limitados (RCPSP) é ampla-
mente investigado na literatura de agendamento, com uma vasta gama de pesquisas dedicadas ao seu
estudo. O presente trabalho apresenta uma implementação desse problema aplicada ao sequencia-
mento de processos quimioterápicos, incorporando restrições de incompatibilidade. Demonstra-se
como o RCPSP com incompatibilidades pode ser útil e eficiente para a otimização de tarefas, eco-
nomia de recursos e aumento da agilidade na aplicação dos tratamentos de quimioterapia.

# Como Executar
1. Clone o repositório:
   ```
   git clone https://github.com/RamonJales/rcpsp-for-quimeoterapy-sequence.git
   cd rcpsp-for-quimeoterapy-sequence
   ```
2. Execute o programa:
   ```
   python main.py
   ```

O algoritmo irá pedir que o usuário digite o número de pacientes que ele quer testar, a partir dos arquivos .sm que existem no trabalho. 
Sobre as incopatibilidades, não há instâncias definidas na literatura à respeito delas. Além disso, elas não está não estão definidas nos
arquivos .sm, sendo assim, o usuário pode alterar o código para testar diversas incompatibilidades diferentes. 

# Referências
- [A System for Generation and Visualization of Resource-Constrained Projects](https://imae.udg.edu/~mbofill/Site/Miquel_Bofills_Home_Page_files/bofill-ccia14.pdf)
- For a more visual introduction, see also this website: [PM Knowledge Center](https://www.pmknowledgecenter.be/dynamic_scheduling/baseline/optimizing-regular-scheduling-objectives-schedule-generation-schemes)
