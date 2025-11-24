use alloc::string::String;

use serde::{Deserialize, Serialize};

use crate::{Event, EventData, EventType, NewProcess, StaticCommandName, Violation};

#[derive(Debug, Deserialize, Serialize)]
pub enum WindowsEventData {
    Violation(Violation),
    NewProcess(NewProcess),
}

#[derive(Debug, Deserialize, Serialize)]
pub struct WindowsEvent {
    pub pid: u32,
    pub name: String,
    pub data: WindowsEventData,
}

impl Into<Event> for WindowsEvent {
    fn into(self) -> Event {
        let pid = self.pid;
        let name = StaticCommandName::from(self.name.as_str());
        let (variant, data) = match self.data {
            WindowsEventData::Violation(violation) => {
                (EventType::Violation, EventData { violation })
            }
            WindowsEventData::NewProcess(new_process) => {
                (EventType::NewProcess, EventData { new_process })
            }
        };

        Event {
            pid,
            name,
            variant,
            data,
        }
    }
}
