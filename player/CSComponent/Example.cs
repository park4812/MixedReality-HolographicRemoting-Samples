using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace CSComponent
{
    public sealed class Example
    {
        int MyNumber;

        public string GetMyString()
        {
            return $"This is call #: {++MyNumber}";
        }
    }
}
