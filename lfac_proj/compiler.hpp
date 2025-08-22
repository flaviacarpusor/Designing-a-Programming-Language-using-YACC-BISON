#include <iostream>     // std::cout, std::cerr
#include <string>       // std::string
#include <vector>       // (optional, daca vrei sa folosesti vectori dinamici)
#include <cstdlib>      // std::exit, EXIT_FAILURE
#include <cmath>        // pentru fmod sau fabs (daca e nevoie)
#include <memory>       // std::unique_ptr / std::shared_ptr (optional, pentru un management mai elegant)

constexpr int LENMAX = 1024;
constexpr int DMAX   = 16;

// Variabile globale pentru domenii (folosim std::string in loc de char[])
std::string domain         = "global";
std::string functionDomain = "global";
std::string paramTemp      = "-";
std::string args;
std::string lvalue;
std::string identif, indexStr;

// Structura „clasa” + array
struct Clasa
{
    std::string name;
};
Clasa classes[LENMAX];
int classNumber = 0;

// Structura pentru variabile
struct VarSymbol
{
    std::string type;   // ex: "int" sau "int[10]"
    std::string name;   // numele variabilei
    std::string value;  // valoarea curenta (ex: "10" sau "0 0 0 ..." pt. array)
    std::string domain; // ex: "global", "nume_clasa", "nume_functie", etc.
    bool isConst;
};
VarSymbol vars[LENMAX];
int varsNumber = 0;

// Structura pentru functii
struct FuncSymbol
{
    std::string returnType;
    std::string name;
    std::string paramList; // ex: "int, bool"
    std::string domain;    // la ce clasa sau context apartine
};
FuncSymbol func[LENMAX];
int funcNumber = 0;

// Enum modern (enum class) pentru tipurile de noduri AST
enum class Category
{
    NUMBER_FLOAT,
    NUMBER_INT,
    NUMBER_BOOL,
    CHAR,
    STRING,
    OPERATOR,
    IDENTIFIER,
    OTHER
};

// Structura pentru nodurile din arbore (AST)
struct AST
{
    std::string label;   // textul asociat (ex: "x", "3.14", "+")
    Category    category; 
    Category    treeType; // tipul dedus dupa analiza
    AST*        left  = nullptr; 
    AST*        right = nullptr;
};

// Structura pentru rezultatul evaluarii unui nod
struct ResultAST
{
    std::string resultStr; 
    Category    treeType;
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII DE CONVERSIE
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

std::string itoaCustom(int value)
{
    return std::to_string(value);
}

std::string ftoaCustom(float value)
{
    // std::to_string(value) de obicei produce ~6 cifre dupa virgula
    return std::to_string(value);
}

// Converteste "true"/"false" in 1/0
int atob(const std::string& valueStr)
{
    return (valueStr == "true") ? 1 : 0;
}

// Stabilim tipul (enum) pe baza stringului (ex: "int" -> NUMBER_INT)
Category convertStringToEnum(const std::string& type)
{
    if (type.find("int")    != std::string::npos) return Category::NUMBER_INT;
    if (type.find("float")  != std::string::npos) return Category::NUMBER_FLOAT;
    if (type.find("bool")   != std::string::npos) return Category::NUMBER_BOOL;
    if (type.find("char")   != std::string::npos) return Category::CHAR;
    if (type.find("string") != std::string::npos) return Category::STRING;
    return Category::OTHER;
}

std::string convertEnumToString(Category category)
{
    switch(category)
    {
        case Category::NUMBER_FLOAT: return "float";
        case Category::NUMBER_INT:   return "int";
        case Category::NUMBER_BOOL:  return "bool";
        case Category::CHAR:         return "char";
        case Category::STRING:       return "string";
        default:                     return "user defined type";
    }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII DE VERIFICARE
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Verifica daca dimensiunea unui array e int > 0
int checkSize(const ResultAST& dim, int yylineno)
{
    if (dim.treeType != Category::NUMBER_INT)
    {
        std::cerr << "[Line " << yylineno << "] Error: Incorrect array dimension.\n";
        std::exit(EXIT_FAILURE);
    }
    int dimInt = std::stoi(dim.resultStr);
    if (dimInt < 1)
    {
        std::cerr << "[Line " << yylineno << "] Error: Incorrect array dimension.\n";
        std::exit(EXIT_FAILURE);
    }
    return dimInt;
}

// Verifica daca o clasa a fost definita
void checkClass(const std::string& name, int yylineno)
{
    int i = 0;
    for (; i < classNumber; i++)
    {
        if (classes[i].name == name) 
            break;
    }
    if (i == classNumber)
    {
        std::cerr << "[Line " << yylineno << "] Error: Class " << name << " is not defined\n";
        std::exit(EXIT_FAILURE);
    }
}

// Verifica indexul unui vector (0 <= index < dimensiune)
void checkValidIndex(const std::string& name, int size, int yylineno)
{
    for (int i = 0; i < varsNumber; i++)
    {
        if (vars[i].name == name)
        {
            // tipul e de forma "int[10]" etc.
            auto pos1 = vars[i].type.find('[');
            auto pos2 = vars[i].type.find(']');
            if (pos1 == std::string::npos || pos2 == std::string::npos) 
                continue;
            std::string inside = vars[i].type.substr(pos1+1, pos2 - (pos1+1));
            int dimInt = std::stoi(inside);
            if (!(0 <= size && size < dimInt))
            {
                std::cerr << "[Line " << yylineno << "] Error: Invalid vector index\n";
                std::exit(EXIT_FAILURE);
            }
        }
    }
}

// Verifica daca paramList coincide cu ce e in args
void compareParamWithArgs(const std::string& functionName, std::string& localArgs, const std::string& dom, int yylineno)
{
    for (int i = 0; i < funcNumber; i++)
    {
        if (func[i].name == functionName && func[i].domain == dom)
        {
            if (func[i].paramList != localArgs)
            {
                std::cerr << "[Line " << yylineno << "] Error: Incorrect parameters passed to the function "
                          << functionName << ".\n";
                std::exit(EXIT_FAILURE);
            }
            // reset
            localArgs.clear(); 
            return;
        }
    }
    std::cerr << "[Line " << yylineno << "] Error: Undefined function " 
              << functionName << " called.\n";
    std::exit(EXIT_FAILURE);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII DE "ADD"
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Adaugam o variabila
void addVar(const std::string& type, const std::string& name, const ResultAST& value, 
            const std::string& dom, bool isConst, int yylineno)
{
    std::string actualDomain = (functionDomain != "global") ? functionDomain : dom;

    for (int i = 0; i < varsNumber; i++)
    {
        if (vars[i].name == name && vars[i].domain == actualDomain)
        {
            std::cerr << "[ERROR] [Line " << yylineno << "] Variable already declared: " << name << "\n";
            std::exit(EXIT_FAILURE);
        }
    }

    vars[varsNumber].type    = type;
    vars[varsNumber].name    = name;
    vars[varsNumber].value   = value.resultStr;
    vars[varsNumber].domain  = actualDomain;
    vars[varsNumber].isConst = isConst;
    varsNumber++;

    std::cout << "[DEBUG] Added variable: " << name << " of type " << type << " in domain " << actualDomain << "\n";
}


// Adaugam un array
void addArray(const std::string& type, const std::string& name, int size, 
              const std::string& dom, bool isConst, int yylineno)
{
    for (int i = 0; i < varsNumber; i++)
    {
        if (vars[i].name == name && vars[i].domain == dom)
        {
            std::cerr << "[Line " << yylineno << "] Error: Variable " 
                      << name << " has already been declared\n";
            std::exit(EXIT_FAILURE);
        }
    }

    vars[varsNumber].type  = type + "[" + std::to_string(size) + "]";
    vars[varsNumber].name  = name;
    std::string initValue  = (type == "bool") ? "false" : "0";

    // Initializare valorica: ex. "0 0 0 ..." sau "false false ..."
    std::string totalValue;
    for (int i = 0; i < size; i++)
    {
        totalValue += initValue + " ";
    }
    if (!totalValue.empty())
        totalValue.pop_back(); // scoatem spatiul de la final

    vars[varsNumber].value   = totalValue;
    vars[varsNumber].domain  = dom;
    vars[varsNumber].isConst = isConst;
    varsNumber++;
}

// Adaugam parametru la paramTemp
void addParameter()
{
    if (paramTemp == "-")
    {
        paramTemp.clear();
    }
    else
    {
        paramTemp += ", ";
    }
    // ultimul element adaugat la vars
    paramTemp += vars[varsNumber - 1].type;
}

// Adaugam functie
void addFunction(const std::string& returnType, const std::string& name, 
                 const std::string& dom, int yylineno)
{
    for (int i = 0; i < funcNumber; i++)
    {
        if (func[i].name == name && func[i].domain == dom)
        {
            std::cerr << "[Line " << yylineno << "] Error: Function " 
                      << name << " has already been declared\n";
            std::exit(EXIT_FAILURE);
        }
    }
    func[funcNumber].returnType = returnType;
    func[funcNumber].name       = name;
    func[funcNumber].paramList  = paramTemp;
    func[funcNumber].domain     = dom;
    paramTemp = "-";
    funcNumber++;
}

// Adaugam o clasa
void addClass(const std::string& name, int yylineno)
{
    for (int i = 0; i < classNumber; i++)
    {
        if (classes[i].name == name)
        {
            std::cerr << "[Line " << yylineno << "] Error: Class " 
                      << name << " has already been defined\n";
            std::exit(EXIT_FAILURE);
        }
    }
    classes[classNumber].name = name;
    classNumber++;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII DE “GET” (accesare)
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Extract ID si index dintr-un string de forma "arr[10]"
void extract_Id_And_Index(const std::string& element, int yylineno)
{
    identif.clear();
    indexStr.clear();

    auto pos1 = element.find('[');
    auto pos2 = element.find(']');

    if (pos1 == std::string::npos || pos2 == std::string::npos)
    {
        std::cerr << "[Line " << yylineno << "] Error: Invalid vector index\n";
        std::exit(EXIT_FAILURE);
    }
    identif  = element.substr(0, pos1);
    std::string digits = element.substr(pos1 + 1, pos2 - (pos1 + 1));

    // verificam sa fie doar cifre
    for (char c : digits)
    {
        if (c < '0' || c > '9')
        {
            std::cerr << "[Line " << yylineno << "] Error: Invalid vector index\n";
            std::exit(EXIT_FAILURE);
        }
    }
    indexStr = digits;
}

std::string getFuncType(const std::string& name)
{
    for (int i = 0; i < funcNumber; i++)
    {
        if (func[i].name == name)
        {
            return func[i].returnType;
        }
    }
    return "?";
}

// Returneaza valoarea actuala a variabilei (ex. "10" sau un element dintr-un array)
std::string getVarValue(const std::string& fullName, int yylineno)
{
    std::cout << "[DEBUG] Searching for variable: '" << fullName 
              << "' at line " << yylineno << "\n";

    // Verificam daca variabila este un array
    bool isArray = (fullName.find('[') != std::string::npos);
    
    if (isArray)
    {
        std::cout << "[DEBUG] Variable '" << fullName << "' appears to be an array.\n";

        extract_Id_And_Index(fullName, yylineno);
        int index = std::stoi(indexStr);
        checkValidIndex(identif, index, yylineno);

        auto getArrayElement = [&](const VarSymbol& v) -> std::string
        {
            std::cout << "[DEBUG] Accessing array '" << v.name 
                      << "' at index " << index << " in domain '" << v.domain << "'\n";

            std::string newValues = v.value;
            int contor = 0;
            size_t start = 0;
            while (true)
            {
                auto pos = newValues.find(' ', start);
                std::string token;
                if (pos != std::string::npos)
                    token = newValues.substr(start, pos - start);
                else
                    token = newValues.substr(start);

                if (contor == index)
                {
                    std::cout << "[DEBUG] Found array element: " << token << "\n";
                    return token;
                }
                contor++;
                if (pos == std::string::npos)
                    break;
                start = pos + 1;
            }

            std::cerr << "[ERROR] Index " << index << " out of bounds in array '" 
                      << v.name << "' at line " << yylineno << "\n";
            return "?";
        };

        // Cautam array-ul in domeniul local
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == domain)
            {
                return getArrayElement(vars[i]);
            }
        }
        // Cautam in domeniul functiei curente
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == functionDomain)
            {
                return getArrayElement(vars[i]);
            }
        }
        // Cautam in domeniul global
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == "global")
            {
                return getArrayElement(vars[i]);
            }
        }

        std::cerr << "[ERROR] Array '" << fullName 
                  << "' not found in any accessible scope at line " << yylineno << "\n";
        return "?";
    }
    else  // Variabila simpla
    {
        std::cout << "[DEBUG] Variable '" << fullName << "' is NOT an array.\n";

        // Cautam in domeniul local
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == domain)
            {
                std::cout << "[DEBUG] Found variable '" << fullName 
                          << "' in local domain: '" << domain 
                          << "' with value: " << vars[i].value << "\n";
                return vars[i].value;
            }
        }

        // Cautam in domeniul functiei curente
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == functionDomain)
            {
                std::cout << "[DEBUG] Found variable '" << fullName 
                          << "' in function domain: '" << functionDomain 
                          << "' with value: " << vars[i].value << "\n";
                return vars[i].value;
            }
        }

        // Cautam in domeniul global
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == "global")
            {
                std::cout << "[DEBUG] Found variable '" << fullName 
                          << "' in global domain with value: " << vars[i].value << "\n";
                return vars[i].value;
            }
        }

        std::cerr << "[ERROR] Variable '" << fullName 
                  << "' is undeclared or out of scope at line " << yylineno << "\n";
        return "?";
    }
}


// Returneaza tipul complet (ex: "int", "int[10]", etc.) al unui obiect
std::string getTypeOfObject(const std::string& name, int yylineno)
{
    for (int i = 0; i < varsNumber; i++)
    {
        if (vars[i].name == name)
        {
            return vars[i].type;
        }
    }
    std::cerr << "[Line " << yylineno << "] Error: Variable " 
              << name << " is not declared\n";
    std::exit(EXIT_FAILURE);
}

// Returneaza tipul efectiv (ex: "int", "float", "bool") al unei variabile sau al unui element de tablou
std::string getVarType(const std::string& fullName)
{
    // variabila simpla
    if (fullName.find('[') == std::string::npos)
    {
        // local
        for (int i = 0; i < varsNumber; i++)
            if (vars[i].name == fullName && vars[i].domain == domain)
                return vars[i].type;
        // functionDomain
        for (int i = 0; i < varsNumber; i++)
            if (vars[i].name == fullName && vars[i].domain == functionDomain)
                return vars[i].type;
        // global
        for (int i = 0; i < varsNumber; i++)
            if (vars[i].name == fullName && vars[i].domain == "global")
                return vars[i].type;
    }
    else
    {
        // e array
        auto pos1 = fullName.find('[');
        if (pos1 == std::string::npos) 
            return "?";
        std::string localIdent = fullName.substr(0, pos1);

        // local
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == localIdent && vars[i].domain == domain)
            {
                if (vars[i].type.find("int")   != std::string::npos) return "int";
                if (vars[i].type.find("float") != std::string::npos) return "float";
                if (vars[i].type.find("bool")  != std::string::npos) return "bool";
            }
        }
        // functionDomain
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == localIdent && vars[i].domain == functionDomain)
            {
                if (vars[i].type.find("int")   != std::string::npos) return "int";
                if (vars[i].type.find("float") != std::string::npos) return "float";
                if (vars[i].type.find("bool")  != std::string::npos) return "bool";
            }
        }
        // global
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == localIdent && vars[i].domain == "global")
            {
                if (vars[i].type.find("int")   != std::string::npos) return "int";
                if (vars[i].type.find("float") != std::string::npos) return "float";
                if (vars[i].type.find("bool")  != std::string::npos) return "bool";
            }
        }
    }
    return "?";
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII DE ACTUALIZARE
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void updateVarValue(const std::string& fullName, const ResultAST& value, int yylineno)
{
    // variabila simpla (fara index)
    if (fullName.find('[') == std::string::npos)
    {
        auto updateSimpleVar = [&](VarSymbol& v)
        {
            if (v.isConst)
            {
                std::cerr << "[Line " << yylineno << "] Error: The value of constant variable "
                          << fullName << " cannot be modified\n";
                std::exit(EXIT_FAILURE);
            }
            if (value.treeType != Category::OTHER &&
                convertStringToEnum(v.type) != value.treeType)
            {
                std::cerr << "[Line " << yylineno << "] Error: The language does not support casting for variable "
                          << fullName << "\n";
                std::exit(EXIT_FAILURE);
            }
            v.value = value.resultStr;
        };

        // local
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == domain)
            {
                updateSimpleVar(vars[i]);
                return;
            }
        }
        // functionDomain
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == functionDomain)
            {
                updateSimpleVar(vars[i]);
                return;
            }
        }
        // global
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == fullName && vars[i].domain == "global")
            {
                updateSimpleVar(vars[i]);
                return;
            }
        }
    }
    else
    {
        // e array
        extract_Id_And_Index(fullName, yylineno);
        int index = std::stoi(indexStr);
        checkValidIndex(identif, index, yylineno);

        auto updateValueInArray = [&](VarSymbol& v)
        {
            if (v.isConst)
            {
                std::cerr << "[Line " << yylineno << "] Error: The value of constant array variable "
                          << v.name << " cannot be modified\n";
                std::exit(EXIT_FAILURE);
            }
            if (value.treeType != Category::OTHER &&
                convertStringToEnum(v.type) != value.treeType)
            {
                std::cerr << "[Line " << yylineno << "] Error: The language does not support casting for variable "
                          << v.name << "\n";
                std::exit(EXIT_FAILURE);
            }

            // despartim prin spatiu, actualizam indexul respectiv
            std::string newValues;
            std::string oldVals = v.value;
            int contor = 0;
            size_t start = 0;
            while (true)
            {
                auto pos = oldVals.find(' ', start);
                std::string token;
                if (pos != std::string::npos)
                    token = oldVals.substr(start, pos - start);
                else
                    token = oldVals.substr(start);

                if (contor == index)
                    newValues += value.resultStr;
                else
                    newValues += token;

                contor++;
                if (pos == std::string::npos)
                    break;
                newValues += " ";
                start = pos + 1;
            }
            v.value = newValues;
        };

        // local
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == domain)
            {
                updateValueInArray(vars[i]);
                return;
            }
        }
        // functionDomain
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == functionDomain)
            {
                updateValueInArray(vars[i]);
                return;
            }
        }
        // global
        for (int i = 0; i < varsNumber; i++)
        {
            if (vars[i].name == identif && vars[i].domain == "global")
            {
                updateValueInArray(vars[i]);
                return;
            }
        }
    }

    // daca nu l-am gasit
    std::cerr << "[Line " << yylineno << "] Error: Undeclared variable " 
              << fullName << " used in expression\n";
    std::exit(EXIT_FAILURE);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                FUNCTII PENTRU AST
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

std::string categoryToString(Category category) {
    switch (category) {
        case Category::NUMBER_FLOAT: return "NUMBER_FLOAT";
        case Category::NUMBER_INT: return "NUMBER_INT";
        case Category::NUMBER_BOOL: return "NUMBER_BOOL";
        case Category::CHAR: return "CHAR";
        case Category::STRING: return "STRING";
        case Category::OPERATOR: return "OPERATOR";
        case Category::IDENTIFIER: return "IDENTIFIER";
        case Category::OTHER: return "OTHER";
        default: return "UNKNOWN";
    }
}

AST* buildTree(const std::string& label, Category category, AST* left, AST* right, int yylineno)
{
    std::cout << "[DEBUG] Building tree for: " << label 
              << " at line " << yylineno 
              << " (left: " << (left ? left->label : "NULL")
              << ", right: " << (right ? right->label : "NULL") << ")\n";

    AST* node = new AST();
    node->label    = label;
    node->category = category;
    node->left     = left;
    node->right    = right;

    if (!node->left && !node->right) 
    {
        if (category == Category::IDENTIFIER) 
        {
            std::string type = getVarType(label);
            if (type == "?") 
            {
                std::cerr << "[ERROR] Undeclared variable '" << label 
                          << "' used in expression at line " << yylineno << "\n";
                std::exit(EXIT_FAILURE);
            } 
            else 
            {
                node->treeType = convertStringToEnum(type);
            }
        } 
        else 
        {
            node->treeType = category;
        }
    }
    else 
    {
        if (node->right && node->left->treeType != node->right->treeType) 
        {
            std::cerr << "[ERROR] Operand types are different: " 
                      << categoryToString(node->left->treeType) << " and " 
                      << categoryToString(node->right->treeType) 
                      << " at line " << yylineno << "\n";
            std::exit(EXIT_FAILURE);
        }
        else 
        {
            node->treeType = node->left->treeType;
        }
    }

    return node;
}



ResultAST evaluateTree(AST* root, int yylineno)
{
    ResultAST res;

    if (!root)
    {
        std::cerr << "[ERROR] Null node encountered in AST evaluation at line " << yylineno << "\n";
        std::exit(EXIT_FAILURE);
    }

    std::cout << "[DEBUG] Evaluating node: " << root->label 
              << " (type: " << categoryToString(root->category) 
              << ") at line " << yylineno << "\n";

    if (root->left && root->right) //  Operator binar
    {
        std::cout << "[DEBUG] Processing binary operator: " << root->label << "\n";

        auto left  = evaluateTree(root->left, yylineno);
        auto right = evaluateTree(root->right, yylineno);

        // Daca oricare operand este necunoscut, afisam detaliile exacte
        if (left.resultStr == "?" || right.resultStr == "?")
        {
            std::cerr << "[Line " << yylineno << "] Error: Undeclared variable used in expression.\n";
            std::cerr << "       - Left operand: " << root->left->label 
                      << " (" << categoryToString(root->left->treeType) << ")\n";
            std::cerr << "       - Right operand: " << root->right->label 
                      << " (" << categoryToString(root->right->treeType) << ")\n";
            std::exit(EXIT_FAILURE);
        }

        if (root->treeType == Category::NUMBER_FLOAT)
        {
            float lv = std::stof(left.resultStr);
            float rv = std::stof(right.resultStr);

            if (root->label == "/" && rv == 0.0f)
            {
                std::cerr << "[Line " << yylineno << "] Error: Division by zero is not possible.\n";
                std::exit(EXIT_FAILURE);
            }

            float rvv = (root->label == "+") ? lv + rv :
                        (root->label == "-") ? lv - rv :
                        (root->label == "*") ? lv * rv :
                        (root->label == "/") ? lv / rv : 0.0f;

            res.resultStr = ftoaCustom(rvv);
        }
        else if (root->treeType == Category::NUMBER_INT)
        {
            int lv = std::stoi(left.resultStr);
            int rv = std::stoi(right.resultStr);

            if ((root->label == "/" || root->label == "%") && rv == 0)
            {
                std::cerr << "[Line " << yylineno << "] Error: Division by zero is not possible.\n";
                std::exit(EXIT_FAILURE);
            }

            int rvv = (root->label == "+") ? lv + rv :
                      (root->label == "-") ? lv - rv :
                      (root->label == "*") ? lv * rv :
                      (root->label == "/") ? lv / rv :
                      (root->label == "%") ? lv % rv : 0;

            res.resultStr = itoaCustom(rvv);
        }
    }
    else if (!root->left)  // 🔹 Frunza (valoare sau variabila)
    {
        std::cout << "[DEBUG] Processing leaf node: " << root->label << "\n";

        if (root->category == Category::NUMBER_INT ||
            root->category == Category::NUMBER_FLOAT ||
            root->category == Category::NUMBER_BOOL ||
            root->category == Category::CHAR ||
            root->category == Category::STRING)
        {
            res.resultStr = root->label;
        }
        else if (root->category == Category::IDENTIFIER)
        {
            std::cout << "[DEBUG] Fetching value for identifier: " << root->label << "\n";
            res.resultStr = getVarValue(root->label, yylineno);

            if (res.resultStr == "?")  // Variabila nu a fost gasita!
            {
                std::cerr << "[Line " << yylineno << "] Error: Undeclared variable '" 
                          << root->label << "' used in expression.\n";
                std::exit(EXIT_FAILURE);
            }
        }
    }

    res.treeType = root->treeType;
    std::cout << "[DEBUG] Evaluation result: " << res.resultStr 
              << " (type: " << categoryToString(res.treeType) 
              << ") at line " << yylineno << "\n";

    return res;
}



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//         FUNCTII „SPECIALE” (Print, TypeOf, etc.)
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void Print(const ResultAST& expr, int yylineno) {
    std::cout << "Function Print was called at line " << yylineno 
              << ". The result is: " << expr.resultStr << std::endl;
}

void TypeOf(const ResultAST& expr, int yylineno)
{
    std::cout << "Function TypeOf was called at line " << yylineno 
              << ". The type is: " << convertEnumToString(expr.treeType) << "\n";
}

// Verifica daca id este atribut in clasa obiectului object
void isIdInClass(const std::string& object, const std::string& id, int yylineno)
{
    std::string clasa = getTypeOfObject(object, yylineno);
    for (int i = 0; i < varsNumber; i++)
    {
        if (vars[i].domain == clasa && vars[i].name == id)
        {
            return;
        }
    }
    std::cerr << "[Line " << yylineno << "] Error: Variable " << id 
              << " is not declared in class " << clasa << "\n";
    std::exit(EXIT_FAILURE);
}

// Verifica daca function este metoda in clasa obiectului object
void isMemberInClass(const std::string& object, const std::string& funcName, int yylineno)
{
    std::string clasa = getTypeOfObject(object, yylineno);
    for (int i = 0; i < funcNumber; i++)
    {
        if (func[i].domain == clasa && func[i].name == funcName)
        {
            return;
        }
    }
    std::cerr << "[Line " << yylineno << "] Error: Function " << funcName 
              << " is not declared in class " << clasa << "\n";
    std::exit(EXIT_FAILURE);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//               FUNCTII DE PRINTARE
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void printAll()
{
    for (int i = 0; i < varsNumber; i++)
    {
        std::cout << (i+1) << ". Name: " << vars[i].name 
                  << ", Type: " << vars[i].type
                  << ", Value: " << vars[i].value
                  << ", Domain: " << vars[i].domain
                  << ", Constant: " << (vars[i].isConst ? "yes" : "no")
                  << "\n";
    }
    for (int i = 0; i < funcNumber; i++)
    {
        std::cout << (i+1) << ". Name: " << func[i].name
                  << ", Returned type: " << func[i].returnType
                  << ", Parameters: " << func[i].paramList
                  << ", Domain: " << func[i].domain
                  << "\n";
    }
}

// In loc de FILE* + fprintf, folosim std::ostream& + << 
void printVar(std::ostream& os)
{
    for (int i = 0; i < varsNumber; i++)
    {
        os << (i+1) << ". Name: " << vars[i].name
           << ", Type: " << vars[i].type
           << ", Value: " << vars[i].value
           << ", Domain: " << vars[i].domain
           << ", Constant: " << (vars[i].isConst ? "yes" : "no")
           << "\n";
    }
}

void printFunc(std::ostream& os)
{
    for (int i = 0; i < funcNumber; i++)
    {
        os << (i+1) << ". Name: " << func[i].name
           << ", Returned type: " << func[i].returnType
           << ", Parameters: " << func[i].paramList
           << ", Domain: " << func[i].domain
           << "\n";
    }
}

