/// \file chwordlebot.cpp
/// \author Chad Hogg
/// \version 2025-10-30
/// I'm sure this has been done a thousand times, but I decided to write a program that plays Wordle.

#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <fstream>

/// \brief The length of words we want to work with.
const std::size_t WORD_LENGTH = 5;
/// \brief The dictionary file that should be used if none is specified.
const std::string DEFAULT_DICT_NAME = "/usr/share/dict/words";

/// \brief A "White" response indicates a letter that does not appear in the word (more times than it already has).
const char NOT_THERE = 'W';
/// \brief A "Yellow" response indicates a letter that appears in the word at a different location.
const char WRONG_SPOT = 'Y';
/// \brief A "Green" response indicates a letter that appears in the word at its current location.
const char RIGHT_SPOT = 'G';


/// \brief A group of words that are potential solutions to a puzzle.
class WordCollection {
public:

    /// \brief Creates a new collection of words by reading from a list.
    /// \param[in] in A stream containing the list of known words.
    /// Strips out any words that are the wrong length of contain non-alphabetic characters.
    /// Converts all lowercase characters to uppercase characters.
    WordCollection (std::istream& in) {
        std::string temp;
        in >> temp;
        while (in) {
            if (temp.length () == WORD_LENGTH) {
                bool good = true;
                for (unsigned short index = 0; index < WORD_LENGTH; ++index) {
                    if (std::islower (temp[index])) {
                        temp[index] = std::toupper (temp[index]);
                    }
                    else if (!std::isalpha (temp[index])) { good = false; }
                }
                if (good) {
                    assert (m_possibleWords.count (temp) == 0);
                    m_possibleWords.insert (temp);
                }
            }
            in >> temp;
        }
    }

    /// \brief Chooses the best word to guess.
    /// \return A word from the collection.
    /// It is assumed that by this point the collection only contains words that are consistent with out knowledge so far.
    /// This uses a heuristc that we would like to include in our guess letters that provide more information,
    ///   and specifically that letter that appear more frequently in our pool of possible guesses are better
    ///   to use than those that appear less frequently.
    /// If multiple words are equally good, it selects between them randomly.
    std::string bestWord () const
    {
        std::unordered_map<char, std::size_t> letterFrequencies;
        for (char c = 'A'; c <= 'Z'; ++c) { letterFrequencies[c] = 0; }
        for (const std::string& word : m_possibleWords) {
            for (char c : word) {
                ++letterFrequencies[c];
            }
        }
        std::unordered_set<std::string> bestOptions;
        std::size_t bestScore = 0;
        for (const std::string& word : m_possibleWords) {
            std::size_t score = 0;
            std::unordered_set<char> alreadySeen;
            for (char c : word) { 
                if (alreadySeen.count (c) == 0) {
                    score += letterFrequencies[c];
                }
                alreadySeen.insert (c);
            }
            if (score > bestScore) {
                bestOptions.clear ();
                bestOptions.insert (word);
                bestScore = score;
            }
            else if (score == bestScore) {
                bestOptions.insert (word);
            }
        }

        std::size_t randomIndex = rand () % bestOptions.size ();
        auto iter = bestOptions.begin ();
        std::advance (iter, randomIndex);
        return *iter;
    }

    /// \brief The collection of words.
    /// This is not private because ConstraintCollection::processNewConstraints() removes things from it.
    /// Maybe they should just be friends.
    std::unordered_set<std::string> m_possibleWords;
};

/// \brief A type of constraint.
/// I was originally using inheritence so that I could have a collection of all constraints of any type.
/// But to get dynamic dispatch, I would need a collection of pointers to them.
/// But of course I wouldn't want to hash the pointers themselves, but the values.
/// And that seemed posssible, but probably not worth the effort.
/// So this base class is entirely vestigial and could be removed.
class Constraint {
public:

    /// \brief Tests whether or not a word satisfies this constraint.
    /// \param[in] str The word to test.
    /// \return True if the word satisfies the constraint; false otherwise.
    virtual bool
    satisfies (const std::string& str) const = 0;
};


/// \brief A constraint regarding a specific location within a word.
/// There are two types: must-match and may-not-match.
class PositionConstraint : public Constraint {
public:

    /// \brief Creates a new position-related constraint.
    /// \param[in] index The position where something should be true.
    /// \param[in] letter The character that might be in that position.
    /// \param[in] shouldMatch True if the character must be in the position; false if it may not be.
    PositionConstraint (unsigned short index, char letter, bool shouldMatch)
    : m_index (index), m_letter (letter), m_shouldMatch (shouldMatch) {
        assert (index < WORD_LENGTH);
        assert (letter >= 'A' && letter <= 'Z');
    }

    virtual bool
    satisfies (const std::string& str) const {
        return (str.at (m_index) == m_letter) == m_shouldMatch;
    }

    bool operator== (const PositionConstraint& other) const {
        return m_index == other.m_index && m_letter == other.m_letter && m_shouldMatch == other.m_shouldMatch;
    }

    const unsigned short m_index;
    const char m_letter;
    const bool m_shouldMatch;
};

std::ostream& operator<< (std::ostream& out, const PositionConstraint& c) {
    if (c.m_shouldMatch) {
        out << "Position " << c.m_index << " must be a " << c.m_letter << ".";
    }
    else {
        out << "Position " << c.m_index << " may not be " << c.m_letter << ".";
    }
    return out;
}


/// \brief A constraint regarding the number of times a letter appears within a word.
/// There are two types: minimum and maximum.
class LetterCountConstraint : public Constraint {
public:

    /// \brief Creates a new count-related constraint.
    /// \param[in] count The number of copies of a character that are allowed.
    /// \param[in] letter The character that should be counted.
    /// \param[in] min True if the we are imposing a lower limit; false if imposing an upper limit.
    LetterCountConstraint (unsigned short count, char letter, bool min)
    : m_count (count), m_letter (letter), m_min (min) {
        assert (m_count < WORD_LENGTH);
        assert (m_letter >= 'A' && m_letter <= 'Z');
    }

    virtual bool
    satisfies (const std::string& str) const {
        unsigned short amt = 0;
        for (char c : str) {
            if (c == m_letter) { ++amt; }
        }
        if (m_min) { return amt >= m_count; }
        else { return amt <= m_count; }
    }

    bool operator== (const LetterCountConstraint& other) const {
        return m_count == other.m_count && m_letter == other.m_letter && m_min == other.m_min;
    }

    const unsigned short m_count;
    const char m_letter;
    const bool m_min;
};

std::ostream& operator<< (std::ostream& out, const LetterCountConstraint& c) {
    if (c.m_min) {
        out << "Word must contain at least " << c.m_count << " copies of " << c.m_letter << ".";
    }
    else {
        out << "Word must contain at most " << c.m_count << " copies of " << c.m_letter << ".";
    }
    return out;
}

template <>
struct std::hash<PositionConstraint>
{
  std::size_t operator()(const PositionConstraint& c) const
  {
    return c.m_index + c.m_letter * 5 + (c.m_shouldMatch ? 1000 : 0);
  }
};

template <>
struct std::hash<LetterCountConstraint>
{
  std::size_t operator()(const LetterCountConstraint& c) const
  {
    return c.m_count + c.m_letter * 5 + (c.m_min ? 1000 : 0);
  }
};


/// \brief A collection of constraints.
/// The constraints are divided into two types (because merging them seemed more trouble than it was worth).
/// Finished constraints are those that all valid words are already satisfying.
/// New constraints are those that have not been checked against words yet.
class ConstraintCollection {
public:

    /// \brief Creates all of the position-related constraints implied by the feedback to a guess.
    /// \param[in] guess The word that was guessed.
    /// \param[in] feedback The response that was received about the guess.
    /// \return A set of all position-related constraints that can be created from this.
    /// If feedback says that a character is in the correct position, we get a constraint that it *must* be there.
    /// If feedback says that a character is in the wrong position, we get a constraint that it *may not* be there.
    static std::unordered_set<PositionConstraint>
    generatePositionConstraints (const std::string& guess, const std::string& feedback) {
        assert (guess.length () == WORD_LENGTH && feedback.length () == WORD_LENGTH);
        for (char c : guess) { assert (c >= 'A' && c <= 'Z'); }
        for (char c : feedback) { assert (c == NOT_THERE || c == WRONG_SPOT || c == RIGHT_SPOT); }
        std::unordered_set<PositionConstraint> constraints;
        for (unsigned short index = 0; index < WORD_LENGTH; ++index) {
            if (feedback[index] == RIGHT_SPOT) {
                constraints.insert ({index, guess[index], true});
            }
            if (feedback[index] == WRONG_SPOT) {
                constraints.insert ({index, guess[index], false});
            }
        }
        return constraints;
    }

    /// \brief Creates all of the count-related constraints implied by the feedback to a guess.
    /// \param[in] guess The word that was guessed.
    /// \param[in] feedback The response that was received about the guess.
    /// \return A set of all count-related constraints that can be created from this.
    /// If feedback says there are X green/yellow copies of a letter, the word must contain at least X copies.
    /// If feedback says there are X green/yellow copies of a letter and not another, the word must contain at most X copies.
    static std::unordered_set<LetterCountConstraint>
    generateLetterCountConstraints (const std::string& guess, const std::string& feedback) {
        assert (guess.length () == WORD_LENGTH && feedback.length () == WORD_LENGTH);
        for (char c : guess) { assert (c >= 'A' && c <= 'Z'); }
        for (char c : feedback) { assert (c == NOT_THERE || c == WRONG_SPOT || c == RIGHT_SPOT); }
        std::unordered_set<LetterCountConstraint> constraints;
        for (unsigned short index = 0; index < WORD_LENGTH; ++index) {
            if (feedback[index] == WRONG_SPOT) {
                unsigned short count = 0;
                for (char c : guess) {
                    if (c == guess[index] && (feedback[index] == WRONG_SPOT || feedback[index] == RIGHT_SPOT)) { ++count; }
                }
                constraints.insert ({count, guess[index], true});
            }
            else if (feedback[index] == NOT_THERE) {
                unsigned short count = 0;
                for (char c : guess) {
                    if (c == guess[index] && (feedback[index] == WRONG_SPOT || feedback[index] == RIGHT_SPOT)) { ++count; }
                }
                constraints.insert ({count, guess[index], false});
            }
        }
        return constraints;
    }

    /// \brief Applies all new constraints to a set of words.
    /// \param[in] candidates The set of words to modify.
    /// Any words for which there is at least one constraint they do not satisfy are removed.
    /// Then all "new" constraints become "finished" constraints.
    void
    processNewConstraints(std::unordered_set<std::string>& candidates) {
        for (auto iter = candidates.begin (); iter != candidates.end (); ) {
            bool good = true;
            for (auto iter2 = m_newPositionConstraints.cbegin (); iter2 != m_newPositionConstraints.cend () && good; ++iter2) {
                if (!iter2->satisfies (*iter)) {
                    //std::cout << "Throwing out " << *iter << " because of " << *iter2 << "\n";
                    good = false;
                }
            }
            for (auto iter2 = m_newLetterCountConstraints.cbegin (); iter2 != m_newLetterCountConstraints.cend () && good; ++iter2) {
                if (!iter2->satisfies (*iter)) {
                    //std::cout << "Throwing out " << *iter << " because of " << *iter2 << "\n";
                    good = false;
                }
            }
            if (!good) { iter = candidates.erase (iter); }
            else { ++iter; }
        }
        m_finishedPositionConstraints.merge (m_newPositionConstraints);
        assert (m_newPositionConstraints.size () == 0);
        m_finishedLetterCountConstraints.merge (m_newLetterCountConstraints);
        assert (m_newLetterCountConstraints.size () == 0);
    }

    void
    addPositionConstraint (const PositionConstraint& c) {
        if (m_finishedPositionConstraints.count (c) == 0) {
            m_newPositionConstraints.insert (c);
        }
    }

    void
    addLetterCountConstraint (const LetterCountConstraint& c) {
        if (m_finishedLetterCountConstraints.count (c) == 0) {
            m_newLetterCountConstraints.insert (c);
        }
    }

private:
    std::unordered_set<PositionConstraint> m_finishedPositionConstraints;
    std::unordered_set<LetterCountConstraint> m_finishedLetterCountConstraints;
    std::unordered_set<PositionConstraint> m_newPositionConstraints;
    std::unordered_set<LetterCountConstraint> m_newLetterCountConstraints;
};


int
main (int argc, char* argv[]) {
    srand (time(NULL));
    std::string dictName = DEFAULT_DICT_NAME;
    if (argc == 2) { dictName = argv[1]; }
    std::ifstream dict (dictName);
    WordCollection words (dict);
    ConstraintCollection constraints;
    int numGuesses = 0;

    while (true) {
        ++numGuesses;
        if (words.m_possibleWords.empty ()) {
            std::cout << "Either your word is not in my dictionary, or you made a mistake.\n";
            break;
        }
        std::string current_guess = words.bestWord ();
        std::cout << "You should guess " << current_guess << "\n";
        std::cout << "Enter a response like GYWWG: ";
        std::string current_feedback;
        std::cin >> current_feedback;
        if (current_feedback == "GGGGG") {
            std::cout << "Yay, we got it in " << numGuesses << " guesses!\n";
            break;
        }
        for (PositionConstraint c : constraints.generatePositionConstraints (current_guess, current_feedback)) {
            constraints.addPositionConstraint (c);
        }
        for (LetterCountConstraint c : constraints.generateLetterCountConstraints (current_guess, current_feedback)) {
            constraints.addLetterCountConstraint (c);
        }
        constraints.processNewConstraints (words.m_possibleWords);
    }
    return 0;
}
