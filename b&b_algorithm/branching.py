class BranchingStrategy:
    """
    Estratégias de branching para o algoritmo B&B
    """
    
    def __init__(self, schedule_scheme):
        self.scheme = schedule_scheme
    
    def select_disjunction(self):
        if not self.scheme.D:
            return None
        
        # Estratégia 1: Menor soma de durações
        best_disj = None
        min_sum = float('inf')
        
        for (i, j) in self.scheme.D:
            p_i = self.scheme.activities[i]['duracao']
            p_j = self.scheme.activities[j]['duracao']
            sum_durations = p_i + p_j
            
            if sum_durations < min_sum:
                min_sum = sum_durations
                best_disj = (i, j)
        
        return best_disj
    
    def select_disjunction_weighted(self, heads, tails):
        if not self.scheme.D:
            return None
        
        best_disj = None
        max_weight = -float('inf')
        
        for (i, j) in self.scheme.D:
            p_i = self.scheme.activities[i]['duracao']
            p_j = self.scheme.activities[j]['duracao']
            
            r_i = heads.get(i, 0)
            r_j = heads.get(j, 0)
            
            # Peso heurístico: considera heads e durações
            weight = abs(r_i - r_j) + (p_i + p_j)
            
            if weight > max_weight:
                max_weight = weight
                best_disj = (i, j)
        
        return best_disj
    
    def create_branches(self, disjunction):
        if disjunction is None:
            return []
        
        i, j = disjunction
        
        # Ramo 1: i → j
        scheme1 = self.scheme.copy()
        scheme1.add_conjunction(i, j)
        scheme1.D.discard((min(i, j), max(i, j)))
        
        # Ramo 2: j → i
        scheme2 = self.scheme.copy()
        scheme2.add_conjunction(j, i)
        scheme2.D.discard((min(i, j), max(i, j)))
        
        return [
            (scheme1, f"{i}→{j}"),
            (scheme2, f"{j}→{i}")
        ]


class SearchNode:
    def __init__(self, schedule_scheme, depth=0, parent=None, branch_decision="root"):
        self.scheme = schedule_scheme
        self.depth = depth
        self.parent = parent
        self.branch_decision = branch_decision
        
        self.lower_bound = float('inf')
        self.upper_bound = float('inf')
        self.is_feasible = True
        self.is_leaf = False
    
    def __lt__(self, other):
        return self.lower_bound < other.lower_bound
    
    def __str__(self):
        return f"Node(depth={self.depth}, LB={self.lower_bound:.1f}, UB={self.upper_bound:.1f}, |D|={len(self.scheme.D)}, decision={self.branch_decision})"
    
    def __repr__(self):
        return self.__str__()
    
    def get_path(self):
        path = []
        node = self
        while node.parent is not None:
            path.append(node.branch_decision)
            node = node.parent
        return list(reversed(path))
