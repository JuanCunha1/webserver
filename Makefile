NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
CPPFLAGS = -Iinclude -MMD -MP


RM = rm -rf

SRC_DIR = src
OBJ_DIR = obj

NETWORK_SRCS = \
	$(SRC_DIR)/network/Socket.cpp \
	$(SRC_DIR)/network/Server.cpp \
	$(SRC_DIR)/network/Client.cpp \
	$(SRC_DIR)/network/ServerManager.cpp \



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
CORE_SRCS = \
	$(SRC_DIR)/core/Router.cpp \
	$(SRC_DIR)/core/FileManager.cpp \
	$(SRC_DIR)/core/CGI.cpp \
	$(SRC_DIR)/core/AutoIndex.cpp

CONFIG_SRCS = \
	$(SRC_DIR)/config/ConfigServer.cpp \
	$(SRC_DIR)/config/ConfigParser.cpp \
	$(SRC_DIR)/config/ConfigParserPost.cpp \
	$(SRC_DIR)/config/ConfigParserPrevious.cpp \
	$(SRC_DIR)/config/ConfigParserTokensBlocks.cpp \
	$(SRC_DIR)/config/ConfigParserTypes.cpp \
	$(SRC_DIR)/config/ConfigParserValidations.cpp


SRCS = \
	$(SRC_DIR)/main.cpp \
	$(NETWORK_SRCS) \
	$(COMMON_SRCS) \
	$(CORE_SRCS) \
	$(CONFIG_SRCS)

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

run: all
	./$(NAME)

debug: CXXFLAGS += -g3
debug: re

.PHONY: all clean fclean re run debug
