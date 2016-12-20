# Copyright 2016 Roland Thiolliere
#
# You can redistribute this file and/or modify it under the terms of
# the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
"""DTW cosine distance and Kullback-Leibler divergence

This is the default distances used by our task. You can start from there
to implement your own.

Be sure to assigne a value in the case the "time" dimension is empty
for one or both of the input arrays. The semantic used here is: the
distance between two empty arrays is 0, and the distance between and
empty array and anything else is +Infinity.

"""

import ABXpy.distances.metrics.dtw as dtw
import ABXpy.distances.metrics.cosine as cosine
import ABXpy.distances.metrics.kullback_leibler as kullback_leibler
import numpy as np


def distance(x, y):
    """Dynamic time warping cosine distance

    The "feature" dimension is along the columns and the "time" dimension
    along the lines of arrays x and y
    """
    if x.shape[0] > 0 and y.shape[0] > 0:
        # x and y are not empty
        d = dtw.dtw(x, y, cosine.cosine_distance,normalized=True)
    elif x.shape[0] == y.shape[0]:
        # both x and y are empty
        d = 0
    else:
        # x or y is empty
        d = np.inf
    return d


def kl_divergence(x, y):
    """Kullback-Leibler divergence"""
    if x.shape[0] > 0 and y.shape[0] > 0:
        d = dtw.dtw(x, y, kullback_leibler.kl_divergence)
    elif x.shape[0] == y.shape[0]:
        d = 0
    else:
        d = np.inf
    return d
