/*************************************************************************/
/*                                                                       */
/* Copyright (c) 1997-98 Richard Tobin, Language Technology Group, HCRC, */
/* University of Edinburgh.                                              */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,     */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF EDINBURGH BE LIABLE */
/* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF    */
/* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION    */
/* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       */
/*                                                                       */
/*************************************************************************/
#include <stdlib.h>
#include "stdio16.h"
#include "system.h"

void *Malloc(int bytes)
{
    void *mem = malloc(bytes);
    if(!mem)
	Fprintf(Stderr, "malloc failed\n");
    return mem;
}

void *Realloc(void *mem, int bytes)
{
    mem = mem ? realloc(mem, bytes) : malloc(bytes);
    if(!mem)
	Fprintf(Stderr, "realloc failed\n");
    return mem;
}

void Free(void *mem)
{
    if (mem != 0) free(mem);
}

