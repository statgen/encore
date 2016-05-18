#pragma once

#ifndef MANIFOLD_HPACK_HPP
#define MANIFOLD_HPACK_HPP

#include <string>
#include <array>
#include <queue>
#include <deque>
#include <list>
#include <map>
#include <unordered_map>


namespace manifold
{
  namespace hpack
  {
    // TODO: Add huffman encoding and clean up this ugly huffman_tree code.
    struct huffman_code2
    {
      const std::uint32_t msb_code;
      const std::uint8_t bit_length;
      huffman_code2(std::uint32_t msb_aligned_int, std::uint8_t bitlength)
        : msb_code(msb_aligned_int), bit_length(bitlength) {}
    };
    class huffman_tree
    {
    private:
      enum class child_direction
      {
        left = 0,
        right
      };
    public:
      huffman_tree(std::initializer_list<std::pair<huffman_code2, char>> il)
      {
        for (auto it = il.begin(); it != il.end(); ++it)
          huffman_tree::insert(this->root_node_, it->first, 31, it->second);
      }

      bool lookup(std::string::const_iterator& input_it, const std::string::const_iterator& input_end, std::uint8_t& bit_position, char& output) const
      {
        bool ret = false;
        auto* current_node = &this->root_node_;
        //std::uint8_t bit_position = 7;
        while (!ret && input_it != input_end)
        {
          child_direction child_to_use = (*input_it >> bit_position) & 0x1 ? child_direction::right : child_direction::left;

          assert(current_node->child(child_to_use) != nullptr);
          if (current_node->child(child_to_use)->is_leaf)
          {
            output = current_node->child(child_to_use)->value;
            ret = true;
          }
          else
          {
            current_node = current_node->child(child_to_use);
          }


          if (bit_position == 0)
          {
            ++input_it;
            bit_position = 7;
          }
          else
          {
            --bit_position;
          }
        }

        return ret;
      }
    private:
      struct node
      {
        bool is_leaf;
        char value;
        node* left_child;
        node* right_child;
        node()
          : is_leaf(false), value('\0'), left_child(nullptr), right_child(nullptr) {}
        node* child(child_direction dir) const { return (dir == child_direction::right ? this->right_child : this->left_child); }
        void set_child(child_direction dir, node* v)
        {
          if (dir == child_direction::left)
            this->left_child = v;
          else
            this->right_child = v;
        }
      };

      node root_node_;




      static bool insert(node& current_node, const huffman_code2& code, std::uint8_t bit_position, char value)
      {
        bool ret = false;
        bool final_bit = (bit_position + code.bit_length) == 32;
        child_direction child_node_to_use = (code.msb_code >> bit_position) & 0x1 ? child_direction::right : child_direction::left;

        if (final_bit)
        {
          assert(current_node.child(child_node_to_use) == nullptr);
          if (current_node.child(child_node_to_use) == nullptr)
          {
            current_node.set_child(child_node_to_use, new node());
            current_node.child(child_node_to_use)->is_leaf = true;
            current_node.child(child_node_to_use)->value = value;
            ret = true;
          }
        }
        else
        {
          if (!current_node.child(child_node_to_use))
            current_node.set_child(child_node_to_use, new node());
          ret = huffman_tree::insert(*current_node.child(child_node_to_use), code, --bit_position, value);
        }

        return ret;
      }
    };

    enum class cacheability
    {
      yes = 1,
      no,
      never
    };

    struct header_field
    {
      std::string name;
      std::string value;
      cacheability cache;
      header_field(std::string&& n, std::string&& v, cacheability cache_field = cacheability::yes)
        : name(std::move(n)), value(std::move(v)), cache(cache_field) {}
      header_field(const std::string& n, const std::string& v, cacheability cache_field = cacheability::yes)
        : name(n), value(v), cache(cache_field) {}
    };

    struct huffman_code
    {
      const std::int32_t msb_code;
      const std::uint8_t bit_length;
      huffman_code(std::int32_t msb_aligned_int, std::uint8_t bitlength)
        : msb_code(msb_aligned_int), bit_length(bitlength) {}
    };

    class huffman_code_cmp
    {
    public:
      bool operator() (const huffman_code& lhc, const huffman_code& rhc) const
      {
        std::uint8_t lowest_bit_length = lhc.bit_length < rhc.bit_length ? lhc.bit_length : rhc.bit_length;
        std::uint32_t mask = 0xFFFFFFFF;
        mask = mask << (32 - lowest_bit_length);

        return ((mask & lhc.msb_code) >> (32 - lowest_bit_length)) < ((mask & rhc.msb_code) >> (32 - lowest_bit_length));
      }
    };

    extern const std::array<std::pair<std::string,std::string>, 61> static_table;
    extern const std::unordered_multimap<std::string, std::size_t> static_table_reverse_lookup_map;
    extern const std::array<std::pair<std::uint32_t, std::uint8_t>,257> huffman_code_array;
    extern const std::map<huffman_code,char,huffman_code_cmp> huffman_code_tree;
    extern const huffman_tree huffman_code_tree2;

    enum class prefix_mask : std::uint8_t
    {
      one_bit    = 0x1,
      two_bit    = 0x3,
      three_bit  = 0x7,
      four_bit   = 0xF,
      five_bit  = 0x1F,
      six_bit   = 0x3F,
      seven_bit = 0x7F,
      eight_bit = 0xFF
    };

    //================================================================//
    class context
    {
    protected:
      std::size_t max_dynamic_table_size_;
      std::size_t current_dynamic_table_size_;
      std::deque<std::pair<std::string,std::string>> dynamic_table_;

      const std::pair<std::string,std::string>& at(std::size_t index) const
      {
        if (!index)
        {
          throw std::out_of_range("Table index cannot be zero.");
        }
        else
        {
          --index;
          if (index < static_table.size())
            return static_table[index];
          else
          {
            index -= static_table.size();
            if (index < this->dynamic_table_.size())
              return this->dynamic_table_[index];
            else
              throw std::out_of_range("Table index out of range.");
          }
        }
      }

      std::size_t dynamic_table_size() const { return this->current_dynamic_table_size_; }
      std::size_t header_list_length() const { return static_table.size() + this->dynamic_table_.size(); }
      void table_evict();
      void table_insert(const std::pair<std::string,std::string>& entry);
      void table_insert(std::pair<std::string,std::string>&& entry);
    public:
      context(std::size_t max_table_size)
        : max_dynamic_table_size_(max_table_size), current_dynamic_table_size_(0) {}
      virtual ~context() {}
    };
    //================================================================//

    //================================================================//
    class encoder : public context
    {
    public:
      struct find_result { std::size_t name_index = 0; std::size_t name_and_value_index = 0; };
      std::queue<std::size_t> table_size_updates_;
      //std::multimap<std::pair<std::string,std::string>, std::size_t> dynamic_table_reverse_lookup_map_;

      static void encode_integer(prefix_mask prfx_mask, std::uint64_t input, std::string& output);
      static void huffman_encode(std::string::const_iterator begin, std::string::const_iterator end, std::string& output);
      find_result find(const header_field& header_to_find);
    public:
      encoder(std::size_t max_table_size)
        : context(max_table_size) {}
      void encode(const std::list<header_field>& headers, std::string& output);
      void add_table_size_update(std::size_t value) { this->table_size_updates_.push(value); }
    };
    //================================================================//

    //================================================================//
    class decoder : public context
    {
    public:
      static std::uint64_t decode_integer(prefix_mask prfx_mask, std::string::const_iterator& itr);
      static bool decode_string_literal(std::string::const_iterator& itr, std::string& output);
      bool decode_nvp(std::size_t table_index, cacheability cache_header, std::string::const_iterator& itr, std::list<header_field>& headers);
      static void huffman_decode(std::string::const_iterator begin, std::string::const_iterator end, std::string& output);
    public:
      decoder(std::size_t max_table_size)
        : context(max_table_size) {}
      bool decode(std::string::const_iterator beg, std::string::const_iterator end, std::list<header_field>& headers); // TODO: Allow for any type of char iterator.
    };
    //================================================================//
  };
}

#endif // MANIFOLD_HPACK_HPP
