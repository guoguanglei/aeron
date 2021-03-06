/*
 * Copyright 2014 - 2017 Real Logic Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "concurrent/aeron_logbuffer_unblocker.h"

bool aeron_logbuffer_unblocker_unblock(
    aeron_mapped_buffer_t *term_buffers,
    aeron_logbuffer_metadata_t *log_meta_data,
    int64_t blocked_position)
{
    const size_t term_length = term_buffers[0].length;
    const size_t position_bits_to_shift = (size_t)aeron_number_of_trailing_zeroes((int32_t)term_length);
    int32_t active_term_count;

    AERON_GET_VOLATILE(active_term_count, log_meta_data->active_term_count);
    const int32_t expected_term_count = (int32_t)(blocked_position >> position_bits_to_shift);

    const size_t index = aeron_logbuffer_index_by_term_count(active_term_count);

    int64_t raw_tail;

    AERON_GET_VOLATILE(raw_tail, log_meta_data->term_tail_counters[index]);
    const int32_t term_id = aeron_logbuffer_term_id(raw_tail);

    if (active_term_count == (expected_term_count - 1) && 0 == (blocked_position & (term_length - 1)))
    {
        return aeron_logbuffer_rotate_log(log_meta_data, active_term_count, term_id);
    }

    const int32_t tail_offset = aeron_logbuffer_term_offset(raw_tail, (int32_t)term_length);
    const int32_t blocked_offset =
        aeron_logbuffer_compute_term_offset_from_position(blocked_position, position_bits_to_shift);

    switch (aeron_term_unblocker_unblock(
        log_meta_data, term_buffers[index].addr, term_length, blocked_offset, tail_offset, term_id))
    {
        case AERON_TERM_UNBLOCKER_STATUS_UNBLOCKED_TO_END:
            aeron_logbuffer_rotate_log(log_meta_data, active_term_count, term_id);
            /* fall through */
        case AERON_TERM_UNBLOCKER_STATUS_UNBLOCKED:
            return true;
            break;
        case AERON_TERM_UNBLOCKER_STATUS_NO_ACTION:
            break;
    }

    return false;
}
