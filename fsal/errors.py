"""Error handling and reporting for FSAL files"""
from typing import Optional, List

class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    RED = '\033[1;31m'
    GREEN = '\033[1;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[1;34m'
    CYAN = '\033[1;36m'

def levenshtein_distance(s1: str, s2: str) -> int:
    """Calculate Levenshtein distance between two strings"""
    if len(s1) == 0:
        return len(s2)
    if len(s2) == 0:
        return len(s1)
    
    matrix = [[0] * (len(s2) + 1) for _ in range(len(s1) + 1)]
    
    for i in range(len(s1) + 1):
        matrix[i][0] = i
    for j in range(len(s2) + 1):
        matrix[0][j] = j
    
    for i in range(1, len(s1) + 1):
        for j in range(1, len(s2) + 1):
            cost = 0 if s1[i-1].lower() == s2[j-1].lower() else 1
            matrix[i][j] = min(
                matrix[i-1][j] + 1,      # deletion
                matrix[i][j-1] + 1,      # insertion
                matrix[i-1][j-1] + cost  # substitution
            )
    
    return matrix[len(s1)][len(s2)]

def find_suggestions(unknown: str, valid_options: List[str], max_distance: int = 2) -> List[str]:
    """Find similar valid options for an unknown keyword"""
    suggestions = []
    best_distance = float('inf')
    
    for option in valid_options:
        dist = levenshtein_distance(unknown, option)
        if dist <= max_distance:
            if dist < best_distance:
                best_distance = dist
                suggestions = [option]
            elif dist == best_distance:
                suggestions.append(option)
    
    return suggestions

class FSALError(Exception):
    """Base class for FSAL errors"""
    def __init__(self, message: str, line: Optional[int] = None, context: Optional[str] = None):
        self.message = message
        self.line = line
        self.context = context
        super().__init__(message)
    
    def format_error(self, error_type: str = "error") -> str:
        output = []
        output.append(f"{Colors.RED}{error_type}{Colors.RESET}: {self.message}")
        
        if self.line is not None:
            output.append(f"  {Colors.BLUE}-->{Colors.RESET} line {self.line}")
            output.append(f"   {Colors.BLUE}|{Colors.RESET}")
            
            if self.context:
                output.append(f"{self.line:2d} {Colors.BLUE}|{Colors.RESET}  {self.context}")
                output.append(f"   {Colors.BLUE}|{Colors.RESET}  {Colors.RED}{'^^' * min(len(self.context), 20)}{Colors.RESET}")
        
        return "\n".join(output)

class FSALSyntaxError(FSALError):
    """Syntax error in FSAL file"""
    pass

class FSALTypeError(FSALError):
    """Type error in FSAL file"""
    def __init__(self, expected: str, got: str, line: Optional[int] = None):
        message = f"expected '{Colors.GREEN}{expected}{Colors.RESET}', found '{Colors.RED}{got}{Colors.RESET}'"
        super().__init__(message, line)

class FSALUnknownKeyError(FSALError):
    """Unknown key or type in FSAL file"""
    def __init__(self, unknown: str, valid_options: Optional[List[str]] = None, line: Optional[int] = None):
        message = f"unknown key '{Colors.BOLD}{unknown}{Colors.RESET}'"
        super().__init__(message, line)
        self.unknown = unknown
        self.suggestions = find_suggestions(unknown, valid_options) if valid_options else []
    
    def format_error(self, error_type: str = "error") -> str:
        output = super().format_error(error_type)
        
        if self.suggestions:
            output += "\n   " + Colors.BLUE + "|" + Colors.RESET + "\n"
            if len(self.suggestions) == 1:
                output += f"   {Colors.BLUE}= {Colors.BOLD}help{Colors.RESET}: did you mean '{Colors.GREEN}{self.suggestions[0]}{Colors.RESET}'?"
            else:
                output += f"   {Colors.BLUE}= {Colors.BOLD}help{Colors.RESET}: did you mean one of these?\n"
                for suggestion in self.suggestions:
                    output += f"           '{Colors.GREEN}{suggestion}{Colors.RESET}'\n"
                output = output.rstrip("\n")
        
        return output

def error_unknown_section(section: str, line: int, valid_sections: List[str]):
    """Report unknown section error"""
    error = FSALUnknownKeyError(section, valid_sections, line)
    print(error.format_error())
    print()

def error_invalid_syntax(message: str, line: int, context: str):
    """Report syntax error"""
    error = FSALSyntaxError(message, line, context)
    print(error.format_error())
    print()

def error_type_mismatch(expected: str, got: str, line: int):
    """Report type mismatch error"""
    error = FSALTypeError(expected, got, line)
    print(error.format_error())
    print()

def error_missing_section(message: str):
    """Report missing section error"""
    print(f"{Colors.RED}error{Colors.RESET}: {message}")
    print()

def warning(message: str, line: Optional[int] = None):
    """Print a warning message"""
    output = f"{Colors.YELLOW}warning{Colors.RESET}: {message}"
    if line is not None:
        output += f"\n  {Colors.BLUE}-->{Colors.RESET} line {line}"
    print(output)
    print()

def info(message: str):
    """Print an info message"""
    print(f"{Colors.CYAN}info{Colors.RESET}: {message}")
    print()
