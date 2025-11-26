class ScheduleScheme:
    """
    Representa um schedule scheme S = (C, D, N, F)
    
    Atributos:
        C: Conjunto de conjunções (precedências fixadas)
        D: Conjunto de disjunções (pares que não podem ser processados em paralelo)
        N: Conjunto de relações de paralelidade
        F: Conjunto de relações de flexibilidade (ainda não decididas)
        activities: Dicionário de atividades
        resources: Dicionário de recursos disponíveis
        incompatibilities: Lista de pares incompatíveis (fixos)
    """
    
    def __init__(self, activities, resources, precedences, incompatibilities=None):
        self.activities = activities
        self.resources = resources
        self.n = len(activities)
        
        # C: Conjunções (precedências)
        self.C = set(precedences)
        
        # D: Disjunções (incompatibilidades são disjunções obrigatórias)
        self.D = set()
        if incompatibilities:
            for (i, j) in incompatibilities:
                # Adiciona como disjunção i ~ j (não ordenada)
                if i in activities and j in activities:
                    self.D.add((min(i, j), max(i, j)))
        
        # N: Relações de paralelidade
        self.N = set()
        
        # F: Relações de flexibilidade (inicialmente todos os pares não definidos)
        self.F = set()
        self._initialize_flexibility_relations()
    
    def _initialize_flexibility_relations(self):
        job_ids = list(self.activities.keys())
        # Remove source e sink
        real_jobs = [j for j in job_ids if j != 1 and j != max(job_ids)]
        
        for i in range(len(real_jobs)):
            for j in range(i + 1, len(real_jobs)):
                job_i = real_jobs[i]
                job_j = real_jobs[j]
                
                # Verifica se já está em C, D ou N
                if not self._is_in_C(job_i, job_j) and \
                   not self._is_in_D(job_i, job_j) and \
                   not self._is_in_N(job_i, job_j):
                    self.F.add((min(job_i, job_j), max(job_i, job_j)))
    
    def _is_in_C(self, i, j):
        """Verifica se existe precedência entre i e j"""
        return (i, j) in self.C or (j, i) in self.C
    
    def _is_in_D(self, i, j):
        """Verifica se existe disjunção entre i e j"""
        return (min(i, j), max(i, j)) in self.D
    
    def _is_in_N(self, i, j):
        """Verifica se existe relação de paralelidade entre i e j"""
        return (min(i, j), max(i, j)) in self.N
    
    def add_conjunction(self, i, j):
        self.C.add((i, j))
        pair = (min(i, j), max(i, j))
        if pair in self.F:
            self.F.remove(pair)
    
    def add_disjunction(self, i, j):
        pair = (min(i, j), max(i, j))
        self.D.add(pair)
        if pair in self.F:
            self.F.remove(pair)
    
    def add_parallelity(self, i, j):
        pair = (min(i, j), max(i, j))
        self.N.add(pair)
        if pair in self.F:
            self.F.remove(pair)
    
    def get_successors(self, i):
        """Retorna lista de sucessores diretos de i"""
        return [j for (pred, j) in self.C if pred == i]
    
    def get_predecessors(self, j):
        """Retorna lista de predecessores diretos de j"""
        return [i for (i, succ) in self.C if succ == j]
    
    def copy(self):
        """Cria uma cópia profunda do schedule scheme"""
        new_scheme = ScheduleScheme(
            self.activities.copy(),
            self.resources.copy(),
            [],
            []
        )
        new_scheme.C = self.C.copy()
        new_scheme.D = self.D.copy()
        new_scheme.N = self.N.copy()
        new_scheme.F = self.F.copy()
        return new_scheme
    
    def is_feasible_schedule(self):
        # Detecção de ciclos usando DFS
        visited = set()
        rec_stack = set()
        
        def has_cycle(node):
            visited.add(node)
            rec_stack.add(node)
            
            for successor in self.get_successors(node):
                if successor not in visited:
                    if has_cycle(successor):
                        return True
                elif successor in rec_stack:
                    return True
            
            rec_stack.remove(node)
            return False
        
        for job_id in self.activities.keys():
            if job_id not in visited:
                if has_cycle(job_id):
                    return False
        
        return True
    
    def __str__(self):
        """Representação em string do schedule scheme"""
        return f"ScheduleScheme(|C|={len(self.C)}, |D|={len(self.D)}, |N|={len(self.N)}, |F|={len(self.F)})"
    
    def __repr__(self):
        return self.__str__()
