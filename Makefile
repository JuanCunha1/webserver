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

PROTOCOL_SRCS = \
	$(SRC_DIR)/protocol/Request.cpp \
	$(SRC_DIR)/protocol/RequestParser.cpp \
	$(SRC_DIR)/protocol/Response.cpp \
	$(SRC_DIR)/protocol/ResponseBuilder.cpp \
	$(SRC_DIR)/protocol/MimeTypes.cpp

CORE_SRCS = \
	$(SRC_DIR)/core/Router.cpp \
	$(SRC_DIR)/core/FileManager.cpp \
	$(SRC_DIR)/core/CGI.cpp \
	$(SRC_DIR)/core/AutoIndex.cpp

CONFIG_SRCS = \
	$(SRC_DIR)/config/Config.cpp \
	$(SRC_DIR)/config/ConfigParser.cpp

SRCS = \
	$(SRC_DIR)/main.cpp \
	$(NETWORK_SRCS) \
	$(PROTOCOL_SRCS) \
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
