NAME        = ./eje
TEST_NAME   = ./parser_test

CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -MMD -MP -g

# Archivos comunes que necesitan tanto el servidor como los tests
COMMON_SRCS = src/protocol/RequestParser.cpp \
              src/protocol/Request.cpp \
              src/protocol/Response.cpp \
              src/protocol/ResponseBuilder.cpp \
              src/protocol/handlers/GetHandler.cpp \
              src/protocol/handlers/PostHandler.cpp \
              src/protocol/handlers/DeleteHandler.cpp \
              src/protocol/handlers/ErrorHandler.cpp \
			  src/protocol/handlers/CgiHandler.cpp \
              src/protocol/MimeTypes.cpp \
			  src/Utils.cpp

# Fuentes del servidor normal
MAIN_SRC    = src/main.cpp
FILES       = $(MAIN_SRC) $(COMMON_SRCS)
OBJ         = $(FILES:.cpp=.o)

# Fuentes del arnés de tests
TEST_SRC    = src/parser_test.cpp
TEST_FILES  = $(TEST_SRC) $(COMMON_SRCS)
TEST_OBJ    = $(TEST_FILES:.cpp=.o)

# Lista completa de dependencias generadas por -MMD
DEPS        = $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

# Regla para compilar y ejecutar los tests directamente
test: $(TEST_NAME)
	@$(TEST_NAME)

$(TEST_NAME): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(TEST_OBJ) -o $(TEST_NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(DEPS)

fclean: clean
	rm -f $(NAME) $(TEST_NAME)

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re test
