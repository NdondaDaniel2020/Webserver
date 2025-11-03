# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: nmatondo <marvin@42.fr>                    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/07/11 10:11:23 by nmatondo          #+#    #+#              #
#    Updated: 2025/11/03 15:54:06 by nmatondo         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# ================================ VARIABLES ================================= #

NAME		= webserv
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -g
INCLUDE		= -I./include
OBJDIR		= obj

# ================================= SOURCES ================================== #

SRCDIR		= src

# Arquivos existentes atualmente
SOURCES		= $(SRCDIR)/main.cpp \
			  $(SRCDIR)/server/Server.cpp
			  $(SRCDIR)/utils/FileUtils.cpp
# Arquivos futuros (descomente conforme forem sendo criados)
# SOURCES	+= $(SRCDIR)/server/Client.cpp \
#			  $(SRCDIR)/server/PollManager.cpp \
#			  $(SRCDIR)/http/Request.cpp \
#			  $(SRCDIR)/http/Response.cpp \
#			  $(SRCDIR)/http/StatusCodes.cpp \
#			  $(SRCDIR)/http/Headers.cpp \
#			  $(SRCDIR)/config/ConfigParser.cpp \
#			  $(SRCDIR)/config/ConfigData.cpp \
#			  $(SRCDIR)/cgi/CGIHandler.cpp \
#			  $(SRCDIR)/cgi/EnvBuilder.cpp \
#			  $(SRCDIR)/utils/Logger.cpp \


OBJECTS		= $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# ================================= HEADERS ================================== #

# Headers existentes atualmente
HEADERS		= include/Server.hpp \
			  include/FileUtils.hpp

# Headers futuros (descomente conforme forem sendo criados)
# HEADERS	+= include/Client.hpp \
#			  include/PollManager.hpp \
#			  include/Request.hpp \
#			  include/Response.hpp \
#			  include/StatusCodes.hpp \
#			  include/Headers.hpp \
#			  include/ConfigParser.hpp \

#			  include/ConfigData.hpp \
#			  include/CGIHandler.hpp \
#			  include/EnvBuilder.hpp \
#			  include/Logger.hpp \

# ================================== RULES =================================== #

all: $(NAME)

$(NAME): $(OBJECTS)
	@echo "🔗 Linking $(NAME)..."
	@$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)
	@echo "✅ $(NAME) compiled successfully!"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	@echo "🔨 Compiling $<..."
	@$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	@echo "🧹 Cleaning object files..."
	@rm -rf $(OBJDIR)

fclean: clean
	@echo "🗑️  Removing $(NAME)..."
	@rm -rf $(NAME)

re: fclean all

# ================================ NEW RULES ================================= #

run: $(NAME)
	@echo "🚀 Running $(NAME)..."
	@./$(NAME) config/default.conf

valgrind: $(NAME)
	@echo "🔍 Running $(NAME) with Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(NAME) config/default.conf

test: $(NAME)
	@echo "🧪 Running tests..."
	@if [ -f tests/test_requests.http ]; then \
		echo "📝 HTTP test file found"; \
	fi
	@if [ -f tests/test_upload.py ]; then \
		echo "🐍 Running upload test..."; \
		python3 tests/test_upload.py; \
	fi
	@if [ -f tests/stress_test.py ]; then \
		echo "💪 Running stress test..."; \
		python3 tests/stress_test.py; \
	fi

debug: CXXFLAGS += -DDEBUG -fsanitize=address -fsanitize=undefined
debug: $(NAME)

help:
	@echo "📖 Available targets:"
	@echo "  all      - Build the project"
	@echo "  clean    - Remove object files"
	@echo "  fclean   - Remove object files and executable"
	@echo "  re       - Rebuild the project"
	@echo "  run      - Build and run the server"
	@echo "  valgrind - Run with Valgrind memory checker"
	@echo "  test     - Run all tests"
	@echo "  debug    - Build with debug flags and sanitizers"
	@echo "  help     - Show this help message"

.PHONY: all clean fclean re run valgrind test debug help