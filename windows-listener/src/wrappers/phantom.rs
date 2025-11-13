use core::marker::PhantomData;
use core::ops::{Deref, DerefMut};

pub struct Lifetime<'a, T> {
    _value: T,
    _marker: PhantomData<&'a T>,
}

impl<T> Lifetime<'_, T> {
    pub fn new(value: T) -> Self {
        Self {
            _value: value,
            _marker: PhantomData,
        }
    }

    /// # Safety
    /// Bringing the inner value out of the lifetime wrapper may violate the lifetime contract.
    pub unsafe fn into_inner(self) -> T {
        self._value
    }
}

impl<T> Deref for Lifetime<'_, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self._value
    }
}

impl<T> DerefMut for Lifetime<'_, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self._value
    }
}

// pub struct Pinned<T>
// where
//     T: Unpin,
// {
//     _value: T,
//     _marker: PhantomPinned,
// }

// impl<T> Pinned<T>
// where
//     T: Unpin,
// {
//     pub fn new(value: T) -> Pin<Box<Self>> {
//         Box::pin(Self {
//             _value: value,
//             _marker: PhantomPinned,
//         })
//     }
// }

// impl<T> Deref for Pinned<T>
// where
//     T: Unpin,
// {
//     type Target = T;

//     fn deref(&self) -> &Self::Target {
//         &self._value
//     }
// }
