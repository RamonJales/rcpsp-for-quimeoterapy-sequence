import time
import heapq
from schedule_scheme import ScheduleScheme
from immediate_selection import ImmediateSelection
from bounds import BoundsCalculator
from branching import BranchingStrategy, SearchNode


class BranchAndBound:
    def __init__(self, activities, resources, precedences, incompatibilities=None, 
                 time_limit=3600, verbose=True):
        self.activities = activities
        self.resources = resources
        self.precedences = precedences
        self.incompatibilities = incompatibilities or []
        self.time_limit = time_limit
        self.verbose = verbose
        
        # Estatísticas
        self.nodes_explored = 0
        self.nodes_pruned = 0
        self.best_solution = None
        self.best_makespan = float('inf')
        self.start_time = None
        
        # Fila de prioridade para best-first search
        self.priority_queue = []
    
    def solve(self):
        self.start_time = time.time()
        
        if self.verbose:
            print("="*80)
            print("BRANCH AND BOUND - RCPSP com Incompatibilidades")
            print("="*80)
            print(f"Atividades: {len(self.activities)}")
            print(f"Recursos: {len(self.resources)}")
            print(f"Precedências: {len(self.precedences)}")
            print(f"Incompatibilidades: {len(self.incompatibilities)}")
            print(f"Limite de tempo: {self.time_limit}s")
            print("="*80)
        
        # Cria schedule scheme inicial
        initial_scheme = ScheduleScheme(
            self.activities,
            self.resources,
            self.precedences,
            self.incompatibilities
        )
        
        # Cria nó raiz
        root_node = SearchNode(initial_scheme, depth=0, parent=None, branch_decision="root")
        
        # Calcula bounds iniciais
        self._compute_bounds(root_node)
        
        if self.verbose:
            print(f"\nNó raiz: LB={root_node.lower_bound:.1f}, UB={root_node.upper_bound:.1f}")
            print(f"Schedule inicial: {root_node.scheme}")
        
        # Inicializa fila de prioridade
        heapq.heappush(self.priority_queue, root_node)
        
        # Loop principal do B&B
        while self.priority_queue:
            # Verifica limite de tempo
            if time.time() - self.start_time > self.time_limit:
                if self.verbose:
                    print(f"\n⏱️  Limite de tempo atingido!")
                break
            
            # Seleciona nó com menor lower bound
            current_node = heapq.heappop(self.priority_queue)
            self.nodes_explored += 1
            
            # Poda por bound
            if current_node.lower_bound >= self.best_makespan:
                self.nodes_pruned += 1
                continue
            
            # Aplica immediate selection
            immediate_sel = ImmediateSelection(current_node.scheme)
            feasible, success = immediate_sel.apply()
            
            if not feasible:
                # Schedule infactível - poda
                self.nodes_pruned += 1
                continue
            
            # Atualiza bounds após immediate selection
            self._compute_bounds(current_node, immediate_sel)
            
            # Verifica se é solução
            if len(current_node.scheme.D) == 0:
                # Todas as disjunções foram resolvidas
                if current_node.upper_bound < self.best_makespan:
                    self.best_makespan = current_node.upper_bound
                    self.best_solution = current_node
                    
                    if self.verbose:
                        elapsed = time.time() - self.start_time
                        print(f"\n✓ Nova melhor solução encontrada!")
                        print(f"  Makespan: {self.best_makespan:.1f}")
                        print(f"  Nós explorados: {self.nodes_explored}")
                        print(f"  Tempo: {elapsed:.2f}s")
                        print(f"  Profundidade: {current_node.depth}")
                
                current_node.is_leaf = True
                continue
            
            # Poda por bound (após immediate selection)
            if current_node.lower_bound >= self.best_makespan:
                self.nodes_pruned += 1
                continue
            
            # Branching
            branching = BranchingStrategy(current_node.scheme)
            
            # Seleciona disjunção para branching
            if hasattr(immediate_sel, 'heads') and immediate_sel.heads:
                disjunction = branching.select_disjunction_weighted(
                    immediate_sel.heads,
                    immediate_sel.tails
                )
            else:
                disjunction = branching.select_disjunction()
            
            if disjunction is None:
                # Não há mais disjunções - solução encontrada
                if current_node.upper_bound < self.best_makespan:
                    self.best_makespan = current_node.upper_bound
                    self.best_solution = current_node
                continue
            
            # Cria ramos
            branches = branching.create_branches(disjunction)
            
            for child_scheme, decision in branches:
                child_node = SearchNode(
                    child_scheme,
                    depth=current_node.depth + 1,
                    parent=current_node,
                    branch_decision=decision
                )
                
                # Calcula bounds do filho
                self._compute_bounds(child_node)
                
                # Adiciona à fila se não for podado
                if child_node.lower_bound < self.best_makespan:
                    heapq.heappush(self.priority_queue, child_node)
                else:
                    self.nodes_pruned += 1
            
            # Imprime progresso periodicamente
            if self.verbose and self.nodes_explored % 100 == 0:
                elapsed = time.time() - self.start_time
                print(f"Nós: {self.nodes_explored:6d} | Podados: {self.nodes_pruned:6d} | "
                      f"Fila: {len(self.priority_queue):5d} | "
                      f"Melhor: {self.best_makespan:6.1f} | "
                      f"Tempo: {elapsed:6.1f}s")
        
        # Estatísticas finais
        elapsed_time = time.time() - self.start_time
        stats = {
            'nodes_explored': self.nodes_explored,
            'nodes_pruned': self.nodes_pruned,
            'time': elapsed_time,
            'optimal': len(self.priority_queue) == 0,
            'best_makespan': self.best_makespan
        }
        
        if self.verbose:
            print("\n" + "="*80)
            print("RESULTADOS FINAIS")
            print("="*80)
            print(f"Makespan ótimo: {self.best_makespan:.1f}")
            print(f"Nós explorados: {self.nodes_explored}")
            print(f"Nós podados: {self.nodes_pruned}")
            print(f"Tempo total: {elapsed_time:.2f}s")
            print(f"Solução ótima: {'Sim' if stats['optimal'] else 'Não (limite de tempo)'}")
            
            if self.best_solution:
                print(f"\nProfundidade da solução: {self.best_solution.depth}")
                print(f"Caminho de decisões: {' → '.join(self.best_solution.get_path()[:10])}...")
            
            print("="*80)
        
        return self.best_makespan, self.best_solution.scheme if self.best_solution else None, stats
    
    def _compute_bounds(self, node, immediate_sel=None):
        bounds_calc = BoundsCalculator(node.scheme)
        
        # Upper bound
        node.upper_bound = bounds_calc.compute_upper_bound()
        
        # Lower bound
        if immediate_sel and hasattr(immediate_sel, 'heads') and immediate_sel.heads:
            # Usa heads e tails se disponíveis
            node.lower_bound = bounds_calc.compute_lower_bound_with_heads_tails(
                immediate_sel.heads,
                immediate_sel.tails
            )
        else:
            # Usa lower bound simples
            node.lower_bound = bounds_calc.compute_lower_bound()
        
        # Lower bound não pode ser maior que upper bound
        if node.lower_bound > node.upper_bound:
            node.lower_bound = node.upper_bound
    
    def get_solution_schedule(self):
        if self.best_solution is None:
            return None
        
        # Usa p-SGS para gerar cronograma da solução
        from algorithm import gerar_cronograma
        
        scheme = self.best_solution.scheme
        precedencias = list(scheme.C)
        incompatibilidades = [(i, j) for (i, j) in scheme.D]
        
        cronograma = gerar_cronograma(
            self.activities,
            self.resources,
            precedencias,
            incompatibilidades
        )
        
        return cronograma


def solve_rcpsp_bb(activities, resources, precedences, incompatibilities=None,
                   time_limit=3600, verbose=True):
    bb = BranchAndBound(
        activities,
        resources,
        precedences,
        incompatibilities,
        time_limit=time_limit,
        verbose=verbose
    )
    
    makespan, scheme, stats = bb.solve()
    cronograma = bb.get_solution_schedule()
    
    return makespan, cronograma, stats
