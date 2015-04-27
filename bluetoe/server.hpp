#ifndef BLUETOE_SERVER_HPP
#define BLUETOE_SERVER_HPP

#include <bluetoe/codes.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/filter.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <cassert>

namespace bluetoe {

    namespace details {
        struct server_name_meta_type;
    }

    /**
     * @brief Root of the declaration of a GATT server.
     *
     * The server serves one or more services configured by the given Options. To configure the server, pass one or more bluetoe::service types as parameters.
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
                bluetoe::no_write_access
            >
        >
    > small_temperature_service;
     * @endcode
     * @sa service
     */
    template < typename ... Options >
    class server
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef typename details::find_all_by_meta_type< details::service_meta_type, Options... >::type services;
        /** @endcond */

        /**
         * @brief per connection data
         *
         * The underlying layer have to provide the memory for a connection and pass the connection_data to l2cap_input().
         * The purpose of this class is to store all connection related data that must be keept per connection and must
         * be reset with a new connection.
         */
        class connection_data : public details::client_characteristic_configurations< details::sum_by< services, details::sum_by_client_configs >::value >
        {
        public:
            /**
             * @brief constructs a connection_data with the maximum transmission unit, the server can provide
             */
            explicit connection_data( std::uint16_t server_mtu );

            /**
             * @brief returns the negotiated MTU
             */
            std::uint16_t negotiated_mtu() const;

            /**
             * @brief sets the MTU size of the connected client.
             *
             * The default is 23. Usually this function will be called by the server implementation as reaction
             * of an "Exchange MTU Request".
             * @post client_mtu() == mtu
             */
            void client_mtu( std::uint16_t mtu );

            /**
             * @brief returns the client MTU
             *
             * By default this returns 23 unless the client MTU was changed by call to client_mtu( std::size_t )
             */
            std::uint16_t client_mtu() const;

            /**
             * @brief returns the MTU of this server as provided in the c'tor
             * @pre connection_data(X).server_mtu() == X
             */
            std::uint16_t server_mtu() const;
        private:
            std::uint16_t   server_mtu_;
            std::uint16_t   client_mtu_;
        };

        /**
         * @brief function to be called by a L2CAP implementation to provide the input from the L2CAP layer and the data assiziate with the connection
         */
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& );

        /**
         * @brief returns the advertising data to the L2CAP implementation
         */
        std::size_t advertising_data( std::uint8_t* buffer, std::size_t buffer_size );

        /**
         * @brief notifies all connected clients about this value
         *
         * There is no check whether there was actual a change to the value or not. It's safe to call this function from a different
         * thread or from an interrupt service routine.
         *
         * The characteristic<> must have been given the notify parameter.
         *
         * Example:
         @code
        std::int32_t temperature;

        typedef bluetoe::server<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
                bluetoe::notify
            >
        > temperature_service;

        int main()
        {
            temperature_service server;

            server.notify( temperature );
        }
        @endcode

         */
        template < class T >
        void notify( const T& value );

    private:
        static constexpr std::size_t number_of_attributes       = details::sum_by< services, details::sum_by_attributes >::value;

        static_assert( std::tuple_size< services >::value > 0, "A server should at least contain one service." );

        static details::attribute attribute_at( std::size_t index );

        bool error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size );
        bool error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size );

        /**
         * for a PDU what starts with an opcode, followed by a pair of handles, the function checks the size of the PDU (must be A or B) and checks the handles.
         * The starting handle must not be 0, must be greate than ending_handle and must be with in the range of attributes available.
         * If the function returns true, everything is fine and starting_handle and ending_handle are filled correctly. Otherwise, an error response was generated.
         */
        template < std::size_t A, std::size_t B = A >
        bool check_size_and_handle_range( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& starting_handle, std::uint16_t& ending_handle );

        template < std::size_t A, std::size_t B = A >
        bool check_size_and_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle );
        bool check_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle );

        void handle_exchange_mtu_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& );
        void handle_find_information_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_find_by_type_value_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_read_by_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_read_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_read_blob_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_read_by_group_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void handle_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        template < class Iterator, class Filter = details::all_uuid_filter >
        void all_attributes( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator&, const Filter& filter = details::all_uuid_filter() );

        template < class Iterator, class Filter = details::all_uuid_filter >
        bool all_services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator&, const Filter& filter = details::all_uuid_filter() );

        std::uint8_t* collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* output, std::uint8_t* output_end );

        static void write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration );
    };

    /**
     * @brief adds additional options to a given server definition
     *
     * example:
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
    ...
    > small_temperature_service;

    typedef bluetoe::extend_server<
        small_temperature_service,
        bluetoe::server_name< name >
    > small_named_temperature_service;

     * @endcode
     * @sa server
     */
    template < typename Server, typename ... Options >
    struct extend_server;

    template < typename ... ServerOptions, typename ... Options >
    struct extend_server< server< ServerOptions... >, Options... > : server< ServerOptions..., Options... >
    {
    };

    /**
     * @brief adds a discoverable device name
     */
    template < const char* const Name >
    struct server_name {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::server_name_meta_type meta_type;

        static constexpr char const* name = Name;
        /** @endcond */
    };

    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < typename ... Options >
    void server< Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& connection )
    {
        // clip the output size to the negotiated mtu
        out_size = std::min< std::size_t >( out_size, connection.negotiated_mtu() );

        assert( in_size != 0 );
        assert( out_size >= details::default_att_mtu_size );

        const details::att_opcodes opcode = static_cast< details::att_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
        case details::att_opcodes::exchange_mtu_request:
            handle_exchange_mtu_request( input, in_size, output, out_size, connection );
            break;
        case details::att_opcodes::find_information_request:
            handle_find_information_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::find_by_type_value_request:
            handle_find_by_type_value_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_by_type_request:
            handle_read_by_type_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_request:
            handle_read_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_blob_request:
            handle_read_blob_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::read_by_group_type_request:
            handle_read_by_group_type_request( input, in_size, output, out_size );
            break;
        case details::att_opcodes::write_request:
            handle_write_request( input, in_size, output, out_size );
            break;
        default:
            error_response( *input, details::att_error_codes::request_not_supported, output, out_size );
            break;
        }
    }

    template < typename ... Options >
    std::size_t server< Options... >::advertising_data( std::uint8_t* begin, std::size_t buffer_size )
    {
        std::uint8_t* const end = begin + buffer_size;

        if ( buffer_size >= 3 )
        {
            begin[ 0 ] = 2;
            begin[ 1 ] = bits( details::gap_types::flags );
            // LE General Discoverable Mode | BR/EDR Not Supported
            begin[ 2 ] = 6;

            begin += 3;
        }

        typedef typename details::find_by_meta_type< details::server_name_meta_type, Options..., server_name< nullptr > >::type name;

        if ( name::name && ( end - begin ) > 2 )
        {
            const std::size_t name_length  = name::name ? std::strlen( name::name ) : 0u;
            const std::size_t max_name_len = std::min< std::size_t >( name_length, end - begin - 2 );

            if ( name_length > 0 )
            {
                begin[ 0 ] = max_name_len + 1;
                begin[ 1 ] = max_name_len == name_length
                    ? bits( details::gap_types::complete_local_name )
                    : bits( details::gap_types::shortened_local_name );

                std::copy( name::name + 0, name::name + max_name_len, &begin[ 2 ] );
                begin += max_name_len + 2;
            }
        }

        return buffer_size - ( end - begin );
    }

    template < typename ... Options >
    details::attribute server< Options... >::attribute_at( std::size_t index )
    {
        return details::attribute_at_list< services, 0 >::attribute_at( index );
    }

    template < typename ... Options >
    bool server< Options... >::error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint16_t handle, std::uint8_t* output, std::size_t& out_size )
    {
        if ( out_size >= 5 )
        {
            output[ 0 ] = bits( details::att_opcodes::error_response );
            output[ 1 ] = opcode;
            output[ 2 ] = handle & 0xff;
            output[ 3 ] = handle >> 8;
            output[ 4 ] = bits( error_code );
            out_size = 5;
        }
        else
        {
            out_size = 0 ;
        }

        return false;
    }

    template < typename ... Options >
    bool server< Options... >::error_response( std::uint8_t opcode, details::att_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
    {
        return error_response( opcode, error_code, 0, output, out_size );
    }

    template < typename ... Options >
    template < std::size_t A, std::size_t B >
    bool server< Options... >::check_size_and_handle_range(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& starting_handle, std::uint16_t& ending_handle )
    {
        if ( in_size != A && in_size != B )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        starting_handle = details::read_handle( &input[ 1 ] );
        ending_handle   = details::read_handle( &input[ 3 ] );

        if ( starting_handle == 0 || starting_handle > ending_handle )
            return error_response( *input, details::att_error_codes::invalid_handle, starting_handle, output, out_size );

        if ( starting_handle > number_of_attributes )
            return error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );

        return true;
    }

    template < typename ... Options >
    template < std::size_t A, std::size_t B >
    bool server< Options... >::check_size_and_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle )
    {
        if ( in_size != A && in_size != B )
            return error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );

        return check_handle( input, in_size, output, out_size, handle );
    }

    template < typename ... Options >
    bool server< Options... >::check_handle( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, std::uint16_t& handle )
    {
        handle = details::read_handle( &input[ 1 ] );

        if ( handle == 0 )
            return error_response( *input, details::att_error_codes::invalid_handle, handle, output, out_size );

        if ( handle > number_of_attributes )
            return error_response( *input, details::att_error_codes::attribute_not_found, handle, output, out_size );

        return true;
    }

    template < typename ... Options >
    void server< Options... >::handle_exchange_mtu_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data& connection )
    {
        if ( in_size != 3 )
        {
            error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );
            return;
        }

        const std::uint16_t mtu = details::read_16bit( input + 1 );

        if ( mtu < details::default_att_mtu_size )
        {
            error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );
            return;
        }

        connection.client_mtu( mtu );

        *output = bits( details::att_opcodes::exchange_mtu_response );
        details::write_16bit( output + 1, connection.server_mtu() );

        out_size = 3u;
    }

    template < typename ... Options >
    void server< Options... >::handle_find_information_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5u >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        const bool only_16_bit_uuids = attribute_at( starting_handle -1 ).uuid != bits( details::gatt_uuids::internal_128bit_uuid );

        std::uint8_t*        write_ptr = &output[ 0 ];
        std::uint8_t* const  write_end = write_ptr + out_size;

        *write_ptr = bits( details::att_opcodes::find_information_response );
        ++write_ptr;

        if ( write_ptr != write_end )
        {
            *write_ptr = bits(
                only_16_bit_uuids
                    ? details::att_uuid_format::short_16bit
                    : details::att_uuid_format::long_128bit );

            ++write_ptr;

        }

        write_ptr = collect_handle_uuid_tuples( starting_handle, ending_handle, only_16_bit_uuids, write_ptr, write_end );

        out_size = write_ptr - &output[ 0 ];
    }

    namespace details {
        struct value_filter
        {
            value_filter( const std::uint8_t* begin, const std::uint8_t* end )
                : begin_( begin )
                , end_( end )
            {
            }

            bool operator()( std::uint16_t index, const details::attribute& attr ) const
            {
                auto read = details::attribute_access_arguments::compare_value( begin_, end_ );
                return attr.access( read, 1 ) == details::attribute_access_result::value_equal;
            }

            const std::uint8_t* const begin_;
            const std::uint8_t* const end_;
        };

        struct collect_find_by_type_groups
        {
            collect_find_by_type_groups( std::uint8_t* begin, std::uint8_t* end )
                : begin_( begin )
                , end_( end )
                , current_( begin )
            {
            }

            template < typename Service >
            bool operator()( std::uint16_t handle, const details::attribute& )
            {
                if ( end_ -  current_ >= 4 )
                {
                    current_ = details::write_handle( current_, handle );
                    current_ = details::write_handle( current_, handle + Service::number_of_attributes - 1 );

                    return true;
                }

                return false;
            }

            std::uint8_t size() const
            {
                return current_ - begin_;
            }

            std::uint8_t* const begin_;
            std::uint8_t* const end_;
            std::uint8_t*       current_;
        };
    }

    template < typename ... Options >
    void server< Options... >::handle_find_by_type_value_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 9u, 23u >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        if ( details::read_handle( &input[ 5 ] ) != bits( details::gatt_uuids::primary_service ) )
        {
            // the spec (v4.2) doesn't define, what to return in this case, but this seems to be a resonable response
            error_response( *input, details::att_error_codes::unsupported_group_type, starting_handle, output, out_size );
            return;
        }

        details::collect_find_by_type_groups iterator( output + 1 , output + out_size );

        if ( all_services_by_group( starting_handle, ending_handle, iterator, details::value_filter( &input[ 7 ], input + in_size ) ) )
        {
            *output  = bits( details::att_opcodes::find_by_type_value_response );
            out_size = iterator.size() + 1;
        }
        else
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }

    }

    template < typename ... Options >
    void server< Options... >::handle_read_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t handle;

        if ( !check_size_and_handle< 3 >( input, in_size, output, out_size, handle ) )
            return;

        auto read = details::attribute_access_arguments::read( output + 1, output + out_size, 0 );
        auto rc   = attribute_at( handle - 1 ).access( read, handle );

        if ( rc == details::attribute_access_result::success || rc == details::attribute_access_result::read_truncated )
        {
            *output  = bits( details::att_opcodes::read_response );
            out_size = 1 + read.buffer_size;
        }
        else
        {
            error_response( *input, details::att_error_codes::read_not_permitted, handle, output, out_size );
        }
    }

    template < typename ... Options >
    void server< Options... >::handle_read_blob_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t handle;

        if ( !check_size_and_handle< 5 >( input, in_size, output, out_size, handle ) )
            return;

        const std::uint16_t offset = details::read_16bit( input + 3 );

        auto read = details::attribute_access_arguments::read( output + 1, output + out_size, offset );
        auto rc   = attribute_at( handle - 1 ).access( read, handle );

        if ( rc == details::attribute_access_result::success || rc == details::attribute_access_result::read_truncated )
        {
            *output  = bits( details::att_opcodes::read_blob_response );
            out_size = 1 + read.buffer_size;
        }
        else if ( rc == details::attribute_access_result::invalid_offset )
        {
            error_response( *input, details::att_error_codes::invalid_offset, handle, output, out_size );
        }
        else
        {
            error_response( *input, details::att_error_codes::read_not_permitted, handle, output, out_size );
        }
     }

    namespace details {
        struct collect_attributes
        {
            void operator()( std::uint16_t handle, const details::attribute& attr )
            {
                static constexpr std::size_t maximum_pdu_size = 253u;
                static constexpr std::size_t header_size      = 2u;

                if ( end_ - current_ >= header_size )
                {
                    const std::size_t max_data_size = std::min< std::size_t >( end_ - current_, maximum_pdu_size + header_size ) - header_size;

                    auto read = attribute_access_arguments::read( current_ + header_size, current_ + header_size + max_data_size, 0 );
                    auto rc   = attr.access( read, handle );

                    if ( rc == details::attribute_access_result::success || rc == details::attribute_access_result::read_truncated && read.buffer_size == maximum_pdu_size )
                    {
                        assert( read.buffer_size <= maximum_pdu_size );

                        if ( first_ )
                        {
                            size_   = read.buffer_size + header_size;
                            first_  = false;
                        }

                        if ( read.buffer_size + header_size == size_ )
                        {
                            current_ = details::write_handle( current_, handle );
                            current_ += static_cast< std::uint8_t >( read.buffer_size );
                        }
                    }
                }
            }

            collect_attributes( std::uint8_t* begin, std::uint8_t* end )
                : begin_( begin )
                , current_( begin )
                , end_( end )
                , size_( 0 )
                , first_( true )
            {
            }

            std::uint8_t size() const
            {
                return current_ - begin_;
            }

            std::uint8_t data_size() const
            {
                return size_;
            }

            bool empty() const
            {
                return current_ == begin_;
            }

            std::uint8_t*   begin_;
            std::uint8_t*   current_;
            std::uint8_t*   end_;
            std::uint8_t    size_;
            bool            first_;
        };
    }

    template < typename ... Options >
    void server< Options... >::handle_read_by_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5 + 2, 5 + 16 >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        details::collect_attributes iterator( output + 2, output + out_size );

        all_attributes( starting_handle, ending_handle, iterator, details::uuid_filter( input + 5, in_size == 5 + 16 ) );

        if ( !iterator.empty() )
        {
            output[ 0 ] = bits( details::att_opcodes::read_by_type_response );
            output[ 1 ] = iterator.data_size();
            out_size = 2 + iterator.size();
        }
        else
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }
    }

    namespace details {
        struct collect_primary_services
        {
            collect_primary_services( std::uint8_t*& output, std::uint8_t* end, std::uint16_t starting_index, std::uint16_t starting_handle, std::uint16_t ending_handle, std::uint8_t& attribute_data_size )
                : output_( output )
                , end_( end )
                , index_( starting_index )
                , starting_handle_( starting_handle )
                , ending_handle_( ending_handle )
                , first_( true )
                , is_128bit_uuid_( true )
                , attribute_data_size_( attribute_data_size )
            {
            }

            template< typename Service >
            void each()
            {
                if ( starting_handle_ <= index_ && index_ <= ending_handle_ )
                {
                    if ( first_ )
                    {
                        is_128bit_uuid_         = Service::uuid::is_128bit;
                        first_                  = false;
                        attribute_data_size_    = is_128bit_uuid_ ? 16 + 4 : 2 + 4;
                    }

                    output_ = Service::read_primary_service_response( output_, end_, index_, is_128bit_uuid_ );
                }

                index_ += Service::number_of_attributes;
            }

                  std::uint8_t*&  output_;
                  std::uint8_t*   end_;
                  std::uint16_t   index_;
            const std::uint16_t   starting_handle_;
            const std::uint16_t   ending_handle_;
                  bool            first_;
                  bool            is_128bit_uuid_;
                  std::uint8_t&   attribute_data_size_;
        };
    }

    template < typename ... Options >
    void server< Options... >::handle_read_by_group_type_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        std::uint16_t starting_handle, ending_handle;

        if ( !check_size_and_handle_range< 5 + 2, 5 + 16 >( input, in_size, output, out_size, starting_handle, ending_handle ) )
            return;

        if ( in_size == 5 + 16 || details::read_handle( &input[ 5 ] ) != bits( details::gatt_uuids::primary_service ) )
        {
            error_response( *input, details::att_error_codes::unsupported_group_type, starting_handle, output, out_size );
            return;
        }

        std::uint8_t*       begin = output;
        std::uint8_t* const end   = output + out_size;

        begin = details::write_opcode( begin, details::att_opcodes::read_by_group_type_response );
        ++begin; // gap for the size

        std::uint8_t* const data_begin = begin;
        details::for_< services >::each( details::collect_primary_services( begin, end, 1, starting_handle, ending_handle, *(begin -1 ) ) );

        if ( begin == data_begin )
        {
            error_response( *input, details::att_error_codes::attribute_not_found, starting_handle, output, out_size );
        }
        else
        {
            out_size = begin - output;
        }
    }

    template < typename ... Options >
    void server< Options... >::handle_write_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        if ( in_size < 3 )
        {
            error_response( *input, details::att_error_codes::invalid_pdu, output, out_size );
            return;
        }

        std::uint16_t handle;

        if ( !check_handle( input, in_size, output, out_size, handle ) )
            return;

        auto write = details::attribute_access_arguments::write( input + 3, input + in_size );
        auto rc    = attribute_at( handle - 1 ).access( write, handle );

        if ( rc == details::attribute_access_result::success )
        {
            *output  = bits( details::att_opcodes::write_response );
            out_size = 1;
        }
        else if ( rc == details::attribute_access_result::write_overflow )
        {
            error_response( *input, details::att_error_codes::invalid_attribute_value_length, handle, output, out_size );
        }
        else
        {
            error_response( *input, details::att_error_codes::write_not_permitted, handle, output, out_size );
        }
    }

    template < typename ... Options >
    template < class Iterator, class Filter >
    void server< Options... >::all_attributes( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iter, const Filter& filter )
    {
        for ( ; starting_handle <= ending_handle && starting_handle <= number_of_attributes; ++starting_handle )
        {
            const details::attribute attr = attribute_at( starting_handle -1 );

            if ( filter( starting_handle, attr ) )
                iter( starting_handle, attr );
        }
    }

    namespace details {
        template < class Iterator, class Filter >
        struct services_by_group
        {
            services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iterator, const Filter& filter, bool& found )
                : starting_handle_( starting_handle )
                , ending_handle_( ending_handle )
                , index_( 1 )
                , iterator_( iterator )
                , filter_( filter )
                , found_( found )
            {

            }

            template< typename Service >
            void each()
            {
                if ( starting_handle_ <= index_ && index_ <= ending_handle_ )
                {
                    const details::attribute& attr = Service::characteristic_declaration_attribute();

                    if ( filter_( index_, attr ) )
                    {
                        found_ = iterator_.template operator()< Service >( index_, attr ) || found_;
                    }
                }

                index_ += Service::number_of_attributes;
            }

            std::uint16_t   starting_handle_;
            std::uint16_t   ending_handle_;
            std::uint16_t   index_;
            Iterator&       iterator_;
            const Filter&   filter_;
            bool&           found_;
        };
    }

    template < typename ... Options >
    template < class Iterator, class Filter >
    bool server< Options... >::all_services_by_group( std::uint16_t starting_handle, std::uint16_t ending_handle, Iterator& iterator, const Filter& filter )
    {
        bool result = false;
        details::for_< services >::each( details::services_by_group< Iterator, Filter >( starting_handle, ending_handle, iterator, filter, result ) );

        return result;
    }

    template < typename ... Options >
    std::uint8_t* server< Options... >::collect_handle_uuid_tuples( std::uint16_t start, std::uint16_t end, bool only_16_bit, std::uint8_t* out, std::uint8_t* out_end )
    {
        const std::size_t size_per_tuple = only_16_bit
            ? 2 + 2
            : 2 + 16;

        for ( ; start <= end && start <= number_of_attributes && out_end - out >= size_per_tuple; ++start )
        {
            const details::attribute attr = attribute_at( start -1 );
            const bool is_16_bit_uuids    = attr.uuid != bits( details::gatt_uuids::internal_128bit_uuid );

            if ( only_16_bit == is_16_bit_uuids )
            {
                details::write_handle( out, start );

                if ( is_16_bit_uuids )
                {
                    details::write_16bit_uuid( out + 2, attr.uuid );
                }
                else
                {
                    write_128bit_uuid( out + 2, attribute_at( start -2 ) );
                }

                out += size_per_tuple;
            }
        }

        return out;
    }

    template < typename ... Options >
    void server< Options... >::write_128bit_uuid( std::uint8_t* out, const details::attribute& char_declaration )
    {
        // this is a little bit tricky: To save memory, details::attribute contains only 16 bit uuids as all
        // but the "Characteristic Value Declaration" contain 16 bit uuids. However, as the "Characteristic Value Declaration"
        // "is the first Attribute after the characteristic declaration", the attribute just in front of the
        // "Characteristic Value Declaration" contains the the 128 bit uuid.
        assert( char_declaration.uuid == bits( details::gatt_uuids::characteristic ) );

        std::uint8_t buffer[ 3 + 16 ];
        auto read = details::attribute_access_arguments::read( buffer, 0 );
        char_declaration.access( read, 1 );

        assert( read.buffer_size == sizeof( buffer ) );

        std::copy( &read.buffer[ 3 ], &read.buffer[ 3 + 16 ], out );
    }

    template < typename ... Options >
    server< Options... >::connection_data::connection_data( std::uint16_t server_mtu )
        : server_mtu_( server_mtu )
        , client_mtu_( details::default_att_mtu_size )
    {
        assert( server_mtu >= details::default_att_mtu_size );
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::connection_data::negotiated_mtu() const
    {
        return std::min( server_mtu_, client_mtu_ );
    }

    template < typename ... Options >
    void server< Options... >::connection_data::client_mtu( std::uint16_t mtu )
    {
        assert( mtu >= details::default_att_mtu_size );
        client_mtu_ = mtu;
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::connection_data::client_mtu() const
    {
        return client_mtu_;
    }

    template < typename ... Options >
    std::uint16_t server< Options... >::connection_data::server_mtu() const
    {
        return server_mtu_;
    }

    /** @endcond */
}

#endif